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

#include <array>
#include <vector>

#include <sstream>

#include <chrono>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-file-properties.hxx"

#include "ptk/ptk-builder.hxx"
#include "ptk/ptk-error.hxx"
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-utils.hxx"

#include "ptk/ptk-app-chooser.hxx"

static constexpr std::array<const std::string_view, 12> chmod_names{
    "owner_r",
    "owner_w",
    "owner_x",
    "group_r",
    "group_w",
    "group_x",
    "others_r",
    "others_w",
    "others_x",
    "set_uid",
    "set_gid",
    "sticky",
};

struct FilePropertiesDialogData
{
    std::filesystem::path dir_path{};
    std::vector<vfs::file_info> file_list{};
    GtkWidget* dlg{nullptr};

    GtkEntry* owner{nullptr};
    GtkEntry* group{nullptr};
    std::string owner_name{};
    std::string group_name{};

    GtkEntry* mtime{nullptr};
    GtkEntry* atime{nullptr};
    std::string orig_mtime{};
    std::string orig_atime{};

    std::array<GtkToggleButton*, 12> chmod_btns;
    std::array<u8, 12> chmod_states;

    GtkLabel* total_size_label{nullptr};
    GtkLabel* size_on_disk_label{nullptr};
    GtkLabel* count_label{nullptr};
    u64 total_size{0};
    u64 size_on_disk{0};
    u64 total_count{0};
    u64 total_count_dir{0};
    bool cancel{false};
    bool done{false};
    GThread* calc_size_thread{nullptr};
    u32 update_label_timer{0};
    GtkWidget* recurse{nullptr};
};

#define FILE_PROPERTIES_DIALOG_DATA(obj) (static_cast<FilePropertiesDialogData*>(obj))

static void on_dlg_response(GtkDialog* dialog, i32 response_id, void* user_data);

const std::vector<std::filesystem::path>
find_subdirectories(const std::filesystem::path& directory, FilePropertiesDialogData* data)
{
    if (data->cancel)
    {
        return {};
    }

    std::vector<std::filesystem::path> subdirectories;

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_directory() && !entry.is_symlink())
        {
            const std::filesystem::path& subdirectory = entry.path();

            subdirectories.emplace_back(subdirectory);
            const auto nested_subdirectories = find_subdirectories(subdirectory, data);
            subdirectories.insert(subdirectories.cend(),
                                  nested_subdirectories.cbegin(),
                                  nested_subdirectories.cend());
        }
    }

    return subdirectories;
}

/*
 * Recursively count total size of all files in the specified directory.
 * If the path specified is a file, the size of the file is directly returned.
 * cancel is used to cancel the operation. This function will check the value
 * pointed by cancel in every iteration. If cancel is set to true, the
 * calculation is cancelled.
 */
static void
calc_total_size_of_files(const std::filesystem::path& path, FilePropertiesDialogData* data)
{
    if (data->cancel)
    {
        return;
    }

    const auto file_stat = ztd::lstat(path);
    if (!file_stat.is_valid())
    {
        return;
    }

    data->total_size += file_stat.size();
    data->size_on_disk += file_stat.blocks() * ztd::BLOCK_SIZE;
    // ++data->total_count;

    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    // recursion time
    std::vector<std::filesystem::path> subdirectories{path};
    const auto found_subdirectories = find_subdirectories(path, data);
    subdirectories.insert(subdirectories.cend(),
                          found_subdirectories.cbegin(),
                          found_subdirectories.cend());

    for (const auto& directory : subdirectories)
    {
        if (data->cancel)
        {
            return;
        }

        for (const auto& directory_file : std::filesystem::directory_iterator(directory))
        {
            if (data->cancel)
            {
                return;
            }

            const auto directory_file_stat = ztd::lstat(directory_file);

            data->total_size += directory_file_stat.size();
            data->size_on_disk += directory_file_stat.blocks() * ztd::BLOCK_SIZE;
            ++data->total_count;
        }
    }
    return;
}

static void*
calc_size(void* user_data)
{
    FilePropertiesDialogData* data = FILE_PROPERTIES_DIALOG_DATA(user_data);
    for (const vfs::file_info file : data->file_list)
    {
        if (data->cancel)
        {
            break;
        }
        const auto path = data->dir_path / file->name();
        calc_total_size_of_files(path, data);
    }
    data->done = true;
    return nullptr;
}

static bool
on_update_labels(FilePropertiesDialogData* data)
{
    const std::string size_str =
        std::format("{} ( {} bytes )", vfs_file_size_format(data->total_size), data->total_size);
    gtk_label_set_text(data->total_size_label, size_str.data());

    const std::string disk_str = std::format("{} ( {} bytes )",
                                             vfs_file_size_format(data->size_on_disk),
                                             data->size_on_disk);
    gtk_label_set_text(data->size_on_disk_label, disk_str.data());

    const std::string count =
        std::format("{} files, {} directories", data->total_count, data->total_count_dir);
    gtk_label_set_text(data->count_label, count.data());

    if (data->done)
    {
        data->update_label_timer = 0;
    }
    return !data->done;
}

static void
on_chmod_btn_toggled(GtkToggleButton* btn, FilePropertiesDialogData* data)
{
    (void)data;
    /* Bypass the default handler */
    g_signal_stop_emission_by_name(btn, "toggled");
    /* Block this handler while we are changing the state of buttons,
      or this handler will be called recursively. */
    g_signal_handlers_block_matched(btn,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_chmod_btn_toggled,
                                    nullptr);

    if (gtk_toggle_button_get_inconsistent(btn))
    {
        gtk_toggle_button_set_inconsistent(btn, false);
        gtk_toggle_button_set_active(btn, false);
    }
    else if (!gtk_toggle_button_get_active(btn))
    {
        gtk_toggle_button_set_inconsistent(btn, true);
    }

    g_signal_handlers_unblock_matched(btn,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_chmod_btn_toggled,
                                      nullptr);
}

static bool
combo_sep(GtkTreeModel* model, GtkTreeIter* it, void* user_data)
{
    (void)user_data;
    for (const auto i : ztd::range(2, 0, -1))
    {
        char* tmp;
        gtk_tree_model_get(model, it, i, &tmp, -1);
        if (tmp)
        {
            std::free(tmp);
            return false;
        }
    }
    return true;
}

static void
on_combo_change(GtkComboBox* combo, void* user_data)
{
    GtkTreeIter it;
    if (gtk_combo_box_get_active_iter(combo, &it))
    {
        char* action;
        GtkTreeModel* model = gtk_combo_box_get_model(combo);
        gtk_tree_model_get(model, &it, 2, &action, -1);
        if (!action)
        {
            vfs::mime_type mime = vfs_mime_type_get_from_type(static_cast<const char*>(user_data));
            GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(combo));
            action = (char*)
                ptk_choose_app_for_mime_type(GTK_WINDOW(parent), mime, false, true, true, true);
            if (action)
            {
                bool exist = false;
                /* check if the action is already in the list */
                if (gtk_tree_model_get_iter_first(model, &it))
                {
                    do
                    {
                        char* tmp;
                        gtk_tree_model_get(model, &it, 2, &tmp, -1);
                        if (!tmp)
                        {
                            continue;
                        }
                        if (ztd::same(tmp, action))
                        {
                            exist = true;
                            std::free(tmp);
                            break;
                        }
                        std::free(tmp);
                    } while (gtk_tree_model_iter_next(model, &it));
                }

                if (!exist) /* It did not exist */
                {
                    const vfs::desktop desktop = vfs_get_desktop(action);

                    GdkPixbuf* icon;
                    icon = desktop->icon(20);
                    gtk_list_store_insert_with_values(GTK_LIST_STORE(model),
                                                      &it,
                                                      0,
                                                      0,
                                                      icon,
                                                      1,
                                                      desktop->display_name().data(),
                                                      2,
                                                      action,
                                                      -1);
                    if (icon)
                    {
                        g_object_unref(icon);
                    }
                    exist = true;
                }

                if (exist)
                {
                    gtk_combo_box_set_active_iter(combo, &it);
                }
                std::free(action);
            }
            else
            {
                const i32 prev_sel =
                    GPOINTER_TO_INT(g_object_get_data(G_OBJECT(combo), "prev_sel"));
                gtk_combo_box_set_active(combo, prev_sel);
            }
        }
        else
        {
            const i32 prev_sel = gtk_combo_box_get_active(combo);
            g_object_set_data(G_OBJECT(combo), "prev_sel", GINT_TO_POINTER(prev_sel));
        }
    }
    else
    {
        g_object_set_data(G_OBJECT(combo), "prev_sel", GINT_TO_POINTER(-1));
    }
}

static GtkWidget*
file_properties_dlg_new(GtkWindow* parent, const std::filesystem::path& dir_path,
                        const std::span<const vfs::file_info> sel_files, i32 page)
{
    GtkBuilder* builder = ptk_gtk_builder_new_from_file(PTK_DLG_FILE_PROPERTIES);
    GtkWidget* dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
    GtkNotebook* notebook = GTK_NOTEBOOK(gtk_builder_get_object(builder, "notebook"));
    xset_set_window_icon(GTK_WINDOW(dlg));

    bool need_calc_size = true;

    const std::string multiple_files = "( multiple files )";
    const char* calculating;
    GtkWidget* name = GTK_WIDGET(gtk_builder_get_object(builder, "file_name"));
    GtkWidget* label_name = GTK_WIDGET(gtk_builder_get_object(builder, "label_filename"));
    GtkWidget* location = GTK_WIDGET(gtk_builder_get_object(builder, "location"));
    gtk_editable_set_editable(GTK_EDITABLE(location), false);
    GtkWidget* target = GTK_WIDGET(gtk_builder_get_object(builder, "target"));
    GtkWidget* label_target = GTK_WIDGET(gtk_builder_get_object(builder, "label_target"));
    gtk_editable_set_editable(GTK_EDITABLE(target), false);
    GtkWidget* mime_type = GTK_WIDGET(gtk_builder_get_object(builder, "mime_type"));
    GtkWidget* open_with = GTK_WIDGET(gtk_builder_get_object(builder, "open_with"));

    bool same_type = true;
    bool is_dirs = false;

    const i32 width = xset_get_int(xset::name::app_dlg, xset::var::s);
    const i32 height = xset_get_int(xset::name::app_dlg, xset::var::z);
    if (width && height)
    {
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, -1);
    }

    const auto data = new FilePropertiesDialogData;
    data->update_label_timer = 0;
    /* FIXME: When will the data be freed??? */
    g_object_set_data(G_OBJECT(dlg), "DialogData", data);
    data->file_list = std::vector<vfs::file_info>(sel_files.begin(), sel_files.end());
    data->dlg = dlg;

    data->dir_path = dir_path;

    gtk_entry_set_text(GTK_ENTRY(location), dir_path.c_str());

    data->total_size_label = GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder, "total_size")));
    data->size_on_disk_label =
        GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder, "size_on_disk")));
    data->count_label = GTK_LABEL(GTK_WIDGET(gtk_builder_get_object(builder, "count")));
    data->owner = GTK_ENTRY(GTK_WIDGET(gtk_builder_get_object(builder, "owner")));
    data->group = GTK_ENTRY(GTK_WIDGET(gtk_builder_get_object(builder, "group")));
    data->mtime = GTK_ENTRY(GTK_WIDGET(gtk_builder_get_object(builder, "mtime")));
    data->atime = GTK_ENTRY(GTK_WIDGET(gtk_builder_get_object(builder, "atime")));

    for (const auto i : ztd::range(magic_enum::enum_count<vfs::chmod_action>()))
    {
        data->chmod_btns[i] = GTK_TOGGLE_BUTTON(
            GTK_WIDGET(gtk_builder_get_object(builder, chmod_names.at(i).data())));
    }

    // MOD
    vfs::mime_type type;
    vfs::mime_type type2 = nullptr;
    for (const vfs::file_info file : sel_files)
    {
        type = file->mime_type();
        if (!type2)
        {
            type2 = file->mime_type();
        }
        if (file->is_directory())
        {
            is_dirs = true;
        }
        if (type != type2)
        {
            same_type = false;
        }
        if (is_dirs && !same_type)
        {
            break;
        }
    }

    data->recurse = GTK_WIDGET(gtk_builder_get_object(builder, "recursive"));
    gtk_widget_set_sensitive(data->recurse, is_dirs);

    vfs::file_info file = sel_files.front();
    if (same_type)
    {
        vfs::mime_type mime = file->mime_type();
        const std::string file_type = std::format("{}\n{}", mime->description(), mime->type());
        gtk_label_set_text(GTK_LABEL(mime_type), file_type.data());
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(mime_type), "( multiple types )");
    }

    /* Open with...
     * Do not show this option menu if files of different types are selected,
     * ,the selected file is a directory, or its type is unknown.
     */
    if (!same_type || file->is_desktop_entry() || file->is_executable())
    {
        /* if open with should not show, destroy it. */
        gtk_widget_destroy(open_with);
        open_with = nullptr;
        gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "open_with_label")));
    }
    else /* Add available actions to the option menu */
    {
        GtkTreeIter it;

        vfs::mime_type mime = file->mime_type();
        const std::vector<std::string> actions = mime->actions();
        GtkCellRenderer* renderer;
        GtkListStore* model;
        gtk_cell_layout_clear(GTK_CELL_LAYOUT(open_with));
        renderer = gtk_cell_renderer_pixbuf_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(open_with), renderer, false);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(open_with), renderer, "pixbuf", 0, nullptr);
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(open_with), renderer, true);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(open_with), renderer, "text", 1, nullptr);
        model = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
        if (!actions.empty())
        {
            for (const std::string_view action : actions)
            {
                const vfs::desktop desktop = vfs_get_desktop(action);
                GdkPixbuf* icon;
                gtk_list_store_append(model, &it);
                icon = desktop->icon(20);
                gtk_list_store_set(model,
                                   &it,
                                   0,
                                   icon,
                                   1,
                                   desktop->display_name().data(),
                                   2,
                                   action.data(),
                                   -1);
                if (icon)
                {
                    g_object_unref(icon);
                }
            }
        }
        else
        {
            g_object_set_data(G_OBJECT(open_with), "prev_sel", GINT_TO_POINTER(-1));
        }

        /* separator */
        gtk_list_store_append(model, &it);

        gtk_list_store_append(model, &it);
        gtk_list_store_set(model, &it, 0, nullptr, 1, "Choose...", -1);
        gtk_combo_box_set_model(GTK_COMBO_BOX(open_with), GTK_TREE_MODEL(model));
        // gtk_combo_box_set_model adds a ref
        g_object_unref(model);
        gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(open_with),
                                             (GtkTreeViewRowSeparatorFunc)combo_sep,
                                             nullptr,
                                             nullptr);
        gtk_combo_box_set_active(GTK_COMBO_BOX(open_with), 0);

        char* mime_data = ztd::strdup(file->mime_type()->type().data());

        g_signal_connect(open_with, "changed", G_CALLBACK(on_combo_change), mime_data);

        /* We can unref mime when combo box gets destroyed */
        g_object_weak_ref(G_OBJECT(open_with), (GWeakNotify)std::free, mime_data);
    }
    g_object_set_data(G_OBJECT(dlg), "open_with", open_with);

    /* Multiple files are selected */
    if (sel_files.size() > 1)
    {
        gtk_widget_set_sensitive(name, false);
        gtk_entry_set_text(GTK_ENTRY(name), multiple_files.data());

        for (const auto i : ztd::range(magic_enum::enum_count<vfs::chmod_action>()))
        {
            gtk_toggle_button_set_inconsistent(data->chmod_btns[i], true);
            data->chmod_states[i] = 2; /* Do not touch this bit */
            g_signal_connect(G_OBJECT(data->chmod_btns[i]),
                             "toggled",
                             G_CALLBACK(on_chmod_btn_toggled),
                             data);
        }
    }
    else
    {
        /* special processing for files with special display names */
        if (file->is_desktop_entry())
        {
            gtk_entry_set_text(GTK_ENTRY(name), file->name().data());
        }
        else
        {
            if (file->is_directory() && !file->is_symlink())
            {
                gtk_label_set_markup_with_mnemonic(GTK_LABEL(label_name),
                                                   "<b>Directory _Name:</b>");
            }
            gtk_entry_set_text(GTK_ENTRY(name), file->display_name().data());
        }

        gtk_editable_set_editable(GTK_EDITABLE(name), false);

        if (!file->is_directory())
        {
            // Only single "file" is selected, so we do not need to
            // caculate total file size
            need_calc_size = false;

            const std::string size =
                std::format("{}  ( {} bytes )", file->display_size(), file->size());
            gtk_label_set_text(data->total_size_label, size.data());

            const std::string on_disk =
                std::format("{}  ( {} bytes )", file->display_disk_size(), file->disk_size());
            gtk_label_set_text(data->size_on_disk_label, on_disk.data());

            gtk_label_set_text(data->count_label, "1 file");
        }

        // Modified / Accessed
        const std::string time_format = "%Y-%m-%d %H:%M:%S";

        // gtk_entry_set_text(GTK_ENTRY(mtime), file->display_mtime());
        const time_t mtime = file->mtime();
        std::tm* local_mtime = std::localtime(&mtime);
        std::ostringstream mtime_formated;
        mtime_formated << std::put_time(local_mtime, time_format.data());

        data->orig_mtime = mtime_formated.str();
        gtk_entry_set_text(GTK_ENTRY(data->mtime), data->orig_mtime.data());

        const time_t atime = file->atime();
        std::tm* local_atime = std::localtime(&atime);
        std::ostringstream atime_formated;
        atime_formated << std::put_time(local_atime, time_format.data());

        data->orig_atime = mtime_formated.str();
        gtk_entry_set_text(GTK_ENTRY(data->atime), data->orig_atime.data());

        // Permissions
        const auto owner_group = ztd::partition(file->display_owner(), ":");

        data->group_name = owner_group[0];
        gtk_entry_set_text(GTK_ENTRY(data->group), data->group_name.data());
        data->owner_name = owner_group[2];
        gtk_entry_set_text(GTK_ENTRY(data->owner), data->owner_name.data());

        for (const auto i : ztd::range(magic_enum::enum_count<vfs::chmod_action>()))
        {
            if (data->chmod_states[i] != 2) /* allow to touch this bit */
            {
                data->chmod_states[i] =
                    ((file->permissions() & chmod_flags.at(i)) != std::filesystem::perms::none ? 1
                                                                                               : 0);
                gtk_toggle_button_set_active(data->chmod_btns[i], data->chmod_states[i]);
            }
        }

        // target
        if (file->is_symlink())
        {
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(label_name), "<b>Link _Name:</b>");
            const auto disp_sym_path = dir_path / file->name();

            try
            {
                auto target_path = std::filesystem::read_symlink(disp_sym_path);

                gtk_entry_set_text(GTK_ENTRY(target), target_path.c_str());

                // relative link to absolute
                if (target_path.is_absolute())
                {
                    target_path = dir_path / target_path;
                }

                if (!std::filesystem::exists(target_path))
                {
                    gtk_label_set_text(GTK_LABEL(mime_type), "( broken link )");
                }
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                ztd::logger::warn("{}", e.what());
                gtk_entry_set_text(GTK_ENTRY(target), "( read link error )");
            }

            gtk_widget_show(target);
            gtk_widget_show(label_target);
        }
    }

    if (need_calc_size)
    {
        /* The total file size displayed in "File Properties" is not
           completely calculated yet. So "Calculating..." is displayed. */
        calculating = "Calculating...";
        gtk_label_set_text(data->total_size_label, calculating);
        gtk_label_set_text(data->size_on_disk_label, calculating);

        g_object_set_data(G_OBJECT(dlg), "calc_size", data);
        data->calc_size_thread = g_thread_new("calc_size", calc_size, data);
        data->update_label_timer = g_timeout_add(250, (GSourceFunc)on_update_labels, data);
    }

    g_signal_connect(dlg, "response", G_CALLBACK(on_dlg_response), dlg);
    g_signal_connect_swapped(gtk_builder_get_object(builder, "ok_button"),
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             dlg);
    g_signal_connect_swapped(gtk_builder_get_object(builder, "cancel_button"),
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy),
                             dlg);

    g_object_unref(builder);

    gtk_notebook_set_current_page(notebook, page);

    if (parent)
    {
        gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);
    }
    return dlg;
}

static uid_t
uid_from_name(const std::string_view user_name)
{
    const auto pw = ztd::passwd(user_name);
    return pw.uid();
}

static gid_t
gid_from_name(const std::string_view group_name)
{
    const auto gr = ztd::group(group_name);
    return gr.gid();
}

static void
on_dlg_response(GtkDialog* dialog, i32 response_id, void* user_data)
{
    (void)user_data;

    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(dialog), &allocation);

    const i32 width = allocation.width;
    const i32 height = allocation.height;
    if (width && height)
    {
        xset_set(xset::name::app_dlg, xset::var::s, std::to_string(width));
        xset_set(xset::name::app_dlg, xset::var::z, std::to_string(height));
    }

    FilePropertiesDialogData* data =
        FILE_PROPERTIES_DIALOG_DATA(g_object_get_data(G_OBJECT(dialog), "DialogData"));
    if (data)
    {
        if (data->update_label_timer)
        {
            g_source_remove(data->update_label_timer);
        }
        data->cancel = true;

        if (data->calc_size_thread)
        {
            g_thread_join(data->calc_size_thread);
        }

        if (response_id == GtkResponseType::GTK_RESPONSE_OK)
        {
            bool mod_change;
            // change file dates
            const char* new_mtime = gtk_entry_get_text(data->mtime);
            if (!(new_mtime && new_mtime[0]) || ztd::same(data->orig_mtime, new_mtime))
            {
                new_mtime = nullptr;
            }
            const char* new_atime = gtk_entry_get_text(data->atime);
            if (!(new_atime && new_atime[0]) || ztd::same(data->orig_atime, new_atime))
            {
                new_atime = nullptr;
            }

            if ((new_mtime || new_atime) && !data->file_list.empty())
            {
                std::string str;
                for (const vfs::file_info file : data->file_list)
                {
                    const auto file_path = data->dir_path / file->name();
                    const std::string quoted_path = ztd::shell::quote(file_path.string());
                    str.append(std::format(" {}", quoted_path));
                }

                std::string cmd;
                if (new_mtime)
                {
                    const std::string quoted_time = ztd::shell::quote(new_mtime);
                    cmd = std::format("touch --no-dereference --no-create -m -d {}{}",
                                      quoted_time,
                                      str);
                }
                if (new_atime)
                {
                    const std::string quoted_time = ztd::shell::quote(new_atime);
                    const std::string quoted_path = cmd; // temp str
                    cmd = std::format("{}{}touch --no-dereference --no-create -a -d {}{}",
                                      cmd,
                                      cmd.empty() ? "" : "\n",
                                      quoted_time,
                                      str);
                }
                if (!cmd.empty())
                {
                    PtkFileTask* ptask =
                        ptk_file_exec_new("Change File Date", "/", GTK_WIDGET(dialog), nullptr);
                    ptask->task->exec_command = cmd;
                    ptask->task->exec_sync = true;
                    ptask->task->exec_export = false;
                    ptask->task->exec_show_output = true;
                    ptask->task->exec_show_error = true;
                    ptk_file_task_run(ptask);
                }
            }

            /* Set default action for mimetype */
            GtkWidget* open_with = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "open_with"));
            if (open_with)
            {
                GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(open_with));
                GtkTreeIter it;

                if (model && gtk_combo_box_get_active_iter(GTK_COMBO_BOX(open_with), &it))
                {
                    char* action;
                    gtk_tree_model_get(model, &it, 2, &action, -1);
                    if (action)
                    {
                        vfs::file_info file = data->file_list.front();
                        vfs::mime_type mime = file->mime_type();
                        mime->set_default_action(action);
                        std::free(action);
                    }
                }
            }

            uid_t uid;
            gid_t gid;

            /* Check if we need chown */
            const char* owner_name = gtk_entry_get_text(data->owner);
            if (owner_name &&
                (data->owner_name.empty() || !ztd::same(owner_name, data->owner_name)))
            {
                uid = uid_from_name(owner_name);
                if (!uid)
                {
                    ptk_show_error(GTK_WINDOW(dialog), "Error", "Invalid User");
                    return;
                }
            }
            const char* group_name = gtk_entry_get_text(data->group);
            if (group_name &&
                (data->group_name.empty() || !ztd::same(group_name, data->group_name)))
            {
                gid = gid_from_name(group_name);
                if (!gid)
                {
                    ptk_show_error(GTK_WINDOW(dialog), "Error", "Invalid Group");
                    return;
                }
            }

            for (const auto i : ztd::range(magic_enum::enum_count<vfs::chmod_action>()))
            {
                if (gtk_toggle_button_get_inconsistent(data->chmod_btns[i]))
                {
                    data->chmod_states[i] = 2; /* Do not touch this bit */
                }
                else if (data->chmod_states[i] != gtk_toggle_button_get_active(data->chmod_btns[i]))
                {
                    mod_change = true;
                    data->chmod_states[i] = gtk_toggle_button_get_active(data->chmod_btns[i]);
                }
                else /* Do not change this bit */
                {
                    data->chmod_states[i] = 2;
                }
            }

            if (!uid || !gid || mod_change)
            {
                std::vector<std::filesystem::path> file_list;
                file_list.reserve(data->file_list.size());
                for (const vfs::file_info file : data->file_list)
                {
                    const auto file_path = data->dir_path / file->name();
                    file_list.emplace_back(file_path);
                }

                PtkFileTask* ptask =
                    ptk_file_task_new(vfs::file_task_type::chmod_chown,
                                      file_list,
                                      GTK_WINDOW(gtk_widget_get_parent(GTK_WIDGET(dialog))),
                                      nullptr);
                // MOD
                ptk_file_task_set_recursive(
                    ptask,
                    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->recurse)));
                if (mod_change)
                {
                    /* If the permissions of file has been changed by the user */
                    ptk_file_task_set_chmod(ptask, data->chmod_states);
                }
                /* For chown */
                ptk_file_task_set_chown(ptask, uid, gid);
                ptk_file_task_run(ptask);

                /*
                 * This file list will be freed by file operation, so we do not
                 * need to free it.
                 */
            }
        }

        /*
         *NOTE: File operation chmod/chown will free the list when it is done,
         *and we only need to free it when there is no file operation applyed.
         */
        delete data;
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

void
ptk_show_file_properties(GtkWindow* parent_win, const std::filesystem::path& cwd,
                         std::vector<vfs::file_info>& sel_files, i32 page)
{
    GtkWidget* dlg;

    if (!sel_files.empty())
    {
        /* Make a copy of the list */
        // for (const vfs::file_info file: sel_files)
        // {
        //     vfs_file_info_ref(file);
        // }

        dlg = file_properties_dlg_new(parent_win, cwd, sel_files, page);
    }
    else
    {
        // no files selected, use cwd as file
        vfs::file_info file = vfs_file_info_new(cwd);
        // sel_files.emplace_back(vfs_file_info_ref(file));
        sel_files.emplace_back(file);
        const auto parent_dir = cwd.parent_path();
        dlg = file_properties_dlg_new(parent_win, parent_dir, sel_files, page);
    }

    // disabling this should not cause leaks since
    // ref count increments are also disabled above?
    // g_signal_connect_swapped(dlg,
    //                         "destroy",
    //                         G_CALLBACK(vfs_file_info_list_free),
    //                         vector_to_glist_VFSFileInfo(sel_files));
    gtk_widget_show(dlg);
}
