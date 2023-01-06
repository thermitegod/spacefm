/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#include <array>
#include <vector>

#include <sstream>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"

#include "ptk/ptk-file-archiver.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-handler.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "settings.hxx"

#include "autosave.hxx"
#include "utils.hxx"

// Archive handlers treeview model enum
enum PTKFileArchiverCol
{
    COL_XSET_NAME,
    COL_HANDLER_NAME
};

// Archive creation handlers combobox model enum
enum PTKFileArchiverExtensionsCol
{
    // COL_XSET_NAME
    COL_HANDLER_EXTENSIONS = 1
};

static const std::string
archive_handler_get_first_extension(xset_t handler_xset)
{
    // Function deals with the possibility that a handler is responsible
    // for multiple MIME types and therefore file extensions. Functions
    // like archive creation need only one extension
    std::string archive_extension;

    if (handler_xset && handler_xset->x)
    {
        // find first extension
        const std::string s = handler_xset->x;
        std::stringstream ss(s);
        std::vector<std::string> pathnames;

        std::string tmp;
        while (std::getline(ss, tmp, ' '))
        {
            pathnames.emplace_back(tmp);
        }

        if (!pathnames.empty())
        {
            for (std::string_view path : pathnames)
            {
                // getting just the extension of the pathname list element
                const auto [filename_no_extension, filename_extension] = get_name_extension(path);
                archive_extension = filename_extension;
                if (!filename_extension.empty())
                {
                    // add a dot to extension
                    archive_extension = fmt::format(".{}", filename_extension);
                    break;
                }
            }
        }
    }
    return archive_extension;
}

static bool
archive_handler_run_in_term(xset_t handler_xset, i32 operation)
{
    // Making sure a valid handler_xset has been passed
    if (!handler_xset)
    {
        LOG_WARN("archive_handler_run_in_term has been called with an "
                 "invalid handler_xset!");
        return false;
    }

    i32 ret;
    switch (operation)
    {
        case PTKFileArchiverArc::ARC_COMPRESS:
            ret = handler_xset->in_terminal;
            break;
        case PTKFileArchiverArc::ARC_EXTRACT:
            ret = handler_xset->keep_terminal;
            break;
        case PTKFileArchiverArc::ARC_LIST:
            ret = handler_xset->scroll_lock;
            break;
        default:
            LOG_WARN("archive_handler_run_in_term was passed an invalid"
                     " archive operation ('{}')!",
                     operation);
            return false;
    }
    return ret == XSetB::XSET_B_TRUE;
}

static void
on_format_changed(GtkComboBox* combo, void* user_data)
{
    // Obtaining reference to dialog
    GtkFileChooser* dlg = GTK_FILE_CHOOSER(user_data);

    // Obtaining new archive filename
    char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
    if (!path)
        return;

    std::string name = Glib::path_get_basename(path);
    free(path);

    // Fetching the combo model
    GtkListStore* list = GTK_LIST_STORE(g_object_get_data(G_OBJECT(dlg), "combo-model"));

    // Attempting to detect and remove extension from any current archive
    // handler - otherwise cycling through the handlers just appends
    // extensions to the filename
    // Obtaining iterator pointing at first handler
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter))
    {
        // Failed to get iterator - warning user and exiting
        LOG_WARN("Unable to get an iterator to the start of the model "
                 "associated with combobox!");
        return;
    }

    // Loop through available handlers
    usize len = 0;
    xset_t handler_xset;
    char* xset_name = nullptr;
    std::string extension;
    do
    {
        gtk_tree_model_get(GTK_TREE_MODEL(list),
                           &iter,
                           PTKFileArchiverCol::COL_XSET_NAME,
                           &xset_name,
                           // PTKFileArchiverExtensionsCol::COL_HANDLER_EXTENSIONS,
                           // &extensions,
                           -1);
        if (xset_name)
        {
            handler_xset = xset_is(xset_name);
            // Obtaining archive extension
            extension = archive_handler_get_first_extension(handler_xset);

            // Checking to see if the current archive filename has this
            if (ztd::endswith(name, extension))
            {
                /* It does - recording its length if its the longest match
                 * yet, and continuing */
                if (extension.size() > len)
                    len = extension.size();
            }
        }
        free(xset_name);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter));

    // Cropping current extension if found
    if (len)
    {
        len = name.size() - len;
        name = name.substr(0, len);
    }

    // Getting at currently selected archive handler
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
    {
        // You have to fetch both items here
        gtk_tree_model_get(GTK_TREE_MODEL(list),
                           &iter,
                           PTKFileArchiverCol::COL_XSET_NAME,
                           &xset_name,
                           // PTKFileArchiverExtensionsCol::COL_HANDLER_EXTENSIONS,
                           // &extensions,
                           -1);
        if (xset_name)
        {
            handler_xset = xset_is(xset_name);
            // Obtaining archive extension
            extension = archive_handler_get_first_extension(handler_xset);

            // Appending extension to original filename
            const std::string new_name = fmt::format("{}{}", name, extension);

            // Updating new archive filename
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), new_name.data());
        }
        free(xset_name);
    }

    // Loading command
    if (handler_xset)
    {
        GtkTextView* view = (GtkTextView*)g_object_get_data(G_OBJECT(dlg), "view");
        std::string error_message;
        std::string command;
        const bool error = ptk_handler_load_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                                   PtkHandlerArchive::HANDLER_COMPRESS,
                                                   handler_xset,
                                                   GTK_TEXT_VIEW(view),
                                                   command,
                                                   error_message);
        if (error)
        {
            xset_msg_dialog(GTK_WIDGET(dlg),
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Error Loading Handler",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            error_message);
        }
    }
}

static const std::string
generate_bash_error_function(bool run_in_terminal, std::string_view parent_quote = "")
{
    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation. Even outside a terminal, IG
     * has requested text is output
     * No translation for security purposes */
    std::string error_pause;
    std::string finished_with_errors;

    if (run_in_terminal)
    {
        error_pause = "read -p";
        finished_with_errors = "[ Finished With Errors ]  Press Enter to close: ";
    }
    else
    {
        error_pause = "echo";
        finished_with_errors = "[ Finished With Errors ]";
    }

    std::string script;
    if (!parent_quote.empty())
    {
        script = fmt::format("fm_handle_err(){{\n"
                             "    fm_err=$?\n"
                             "    rmdir --ignore-fail-on-non-empty {}\n"
                             "    if [ $fm_err -ne 0 ];then\n"
                             "       echo;{} \"{}\"\n"
                             "       exit $fm_err\n"
                             "    fi\n"
                             "}}",
                             parent_quote,
                             error_pause,
                             finished_with_errors);
    }
    else
    {
        script = fmt::format("fm_handle_err(){{\n"
                             "    fm_err=$?\n"
                             "    if [ $fm_err -ne 0 ];then\n"
                             "       echo;{} \"{}\"\n"
                             "       exit $fm_err\n"
                             "    fi\n"
                             "}}",
                             error_pause,
                             finished_with_errors);
    }

    return script;
}

static const std::string
replace_archive_subs(std::string_view line, std::string_view n, std::string_view N,
                     std::string_view o, std::string_view x, std::string_view g)
{
    std::string new_line = line.data();

    new_line = ztd::replace(new_line, "%n", n);
    new_line = ztd::replace(new_line, "%N", N);
    new_line = ztd::replace(new_line, "%o", o);
    new_line = ztd::replace(new_line, "%O", o);
    new_line = ztd::replace(new_line, "%x", x);
    new_line = ztd::replace(new_line, "%g", g);
    new_line = ztd::replace(new_line, "%G", g);

    // double percent %% - reduce to single
    new_line = ztd::replace(new_line, "%%", "%");

    return new_line;
}

void
ptk_file_archiver_create(PtkFileBrowser* file_browser, const std::vector<vfs::file_info>& sel_files,
                         std::string_view cwd)
{
    /* Generating dialog - extra nullptr on the nullptr-terminated list to
     * placate an irrelevant compilation warning. See notes in
     * ptk-handler.c:ptk_handler_show_config about GTK failure to
     * identify top-level widget */
    GtkWidget* top_level =
        file_browser ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser->main_window)) : nullptr;
    GtkWidget* dlg = gtk_file_chooser_dialog_new("Create Archive",
                                                 top_level ? GTK_WINDOW(top_level) : nullptr,
                                                 GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE,
                                                 nullptr,
                                                 nullptr);

    /* Adding standard buttons and saving references in the dialog
     * 'Configure' button has custom text but a stock image */
    GtkButton* btn_configure = GTK_BUTTON(
        gtk_dialog_add_button(GTK_DIALOG(dlg), "Conf_igure", GtkResponseType::GTK_RESPONSE_NONE));
    g_object_set_data(G_OBJECT(dlg), "btn_configure", GTK_BUTTON(btn_configure));
    g_object_set_data(
        G_OBJECT(dlg),
        "btn_cancel",
        gtk_dialog_add_button(GTK_DIALOG(dlg), "Cancel", GtkResponseType::GTK_RESPONSE_CANCEL));
    g_object_set_data(
        G_OBJECT(dlg),
        "btn_ok",
        gtk_dialog_add_button(GTK_DIALOG(dlg), "OK", GtkResponseType::GTK_RESPONSE_OK));

    GtkFileFilter* filter = gtk_file_filter_new();

    /* Top hbox has 'Command:' label, 'Archive Format:' label then format
     * combobox */
    GtkWidget* hbox_top = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* lbl_command = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_command), "Co_mpress Commands:");
    gtk_box_pack_start(GTK_BOX(hbox_top), lbl_command, false, true, 2);

    // Generating a ComboBox with model behind, and saving model for use
    // in callback - now that archive handlers are custom, cannot rely on
    // presence or a particular order
    // Model is xset name then extensions the handler deals with
    GtkListStore* list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget* combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list));
    // gtk_combo_box_new_with_model adds a ref
    g_object_unref(list);
    g_object_set_data(G_OBJECT(dlg), "combo-model", (void*)list);

    // Need to manually create the combobox dropdown cells!! Mapping the
    // extensions column from the model to the displayed cell
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, true);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo),
                                   renderer,
                                   "text",
                                   PTKFileArchiverExtensionsCol::COL_HANDLER_EXTENSIONS,
                                   nullptr);

    // Fetching available archive handlers
    const std::string archive_handlers_s = xset_get_s(XSetName::ARC_CONF2);

    // Dealing with possibility of no handlers
    if (ztd::compare(archive_handlers_s, "") <= 0)
    {
        /* Telling user to ensure handlers are available and bringing
         * up configuration */
        xset_msg_dialog(GTK_WIDGET(dlg),
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Archive Handlers - Create Archive",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        "No archive handlers "
                        "configured. You must add a handler before "
                        "creating an archive.");
        ptk_handler_show_config(PtkHandlerMode::HANDLER_MODE_ARC, file_browser, nullptr);
        return;
    }

    // Splitting archive handlers
    const std::vector<std::string> archive_handlers = ztd::split(archive_handlers_s, " ");

    // Debug code
    // LOG_INFO("archive_handlers_s: {}", archive_handlers_s);

    // Looping for handlers (nullptr-terminated list)
    GtkTreeIter iter;
    xset_t handler_xset;
    // Get xset name of last used handler
    char* xset_name = xset_get_s(XSetName::ARC_DLG); // do not free
    i32 format = 4;                                  // default tar.gz
    i32 n = 0;
    for (std::string_view archive_handler : archive_handlers)
    {
        if (archive_handler.empty())
            continue;

        // Fetching handler
        handler_xset = xset_is(archive_handler);

        if (handler_xset && handler_xset->b == XSetB::XSET_B_TRUE)
        /* Checking to see if handler is enabled, can cope with
         * compression and the extension is set - dealing with empty
         * command yet 'run in terminal' still ticked
                               && handler_xset->y
                               && !ztd::same(handler_xset->y, "") != 0
                               && !ztd::same(handler_xset->y, "+") != 0
                               && !ztd::same(handler_xset->x, "") != 0) */
        {
            /* Adding to filter so that only relevant archives
             * are displayed when the user chooses an archive name to
             * create. Note that the handler may be responsible for
             * multiple MIME types and extensions */
            gtk_file_filter_add_mime_type(filter, handler_xset->s);

            // Appending to combobox
            // Obtaining appending iterator for model
            gtk_list_store_append(GTK_LIST_STORE(list), &iter);

            // Adding to model
            const std::string extensions =
                fmt::format("{} ( {} ) ", handler_xset->menu_label, handler_xset->x);
            gtk_list_store_set(GTK_LIST_STORE(list),
                               &iter,
                               PTKFileArchiverCol::COL_XSET_NAME,
                               archive_handlers.data(),
                               PTKFileArchiverExtensionsCol::COL_HANDLER_EXTENSIONS,
                               extensions.data(),
                               -1);

            // Is last used handler?
            if (ztd::same(xset_name, handler_xset->name))
                format = n;
            n++;
        }
    }

    // Applying filter
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dlg), filter);

    // Restoring previous selected handler
    xset_name = nullptr;
    n = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)), nullptr);
    if (format < 0 || format > n - 1)
        format = 0;
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), format);

    // Adding filter box to hbox and connecting callback
    g_signal_connect(combo, "changed", G_CALLBACK(on_format_changed), dlg);
    gtk_box_pack_end(GTK_BOX(hbox_top), combo, false, false, 2);

    GtkWidget* lbl_archive_format = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_archive_format), "_Archive Format:");
    gtk_box_pack_end(GTK_BOX(hbox_top), lbl_archive_format, false, false, 2);
    gtk_widget_show_all(hbox_top);

    GtkTextView* view = (GtkTextView*)gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GtkWrapMode::GTK_WRAP_WORD_CHAR);
    GtkWidget* view_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view_scroll), GTK_WIDGET(view));
    g_object_set_data(G_OBJECT(dlg), "view", view);

    std::string command;
    std::string compress_command;
    std::string error_message;
    bool error = false;

    /* Loading command for handler, based off the format handler */
    // Obtaining iterator from string turned into a path into the model
    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list),
                                            &iter,
                                            std::to_string(format).data()))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(list),
                           &iter,
                           PTKFileArchiverCol::COL_XSET_NAME,
                           &xset_name,
                           // PTKFileArchiverExtensionsCol::COL_HANDLER_EXTENSIONS,
                           // &extensions,
                           -1);
        if (xset_name)
        {
            handler_xset = xset_is(xset_name);

            error = ptk_handler_load_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                            PtkHandlerArchive::HANDLER_COMPRESS,
                                            handler_xset,
                                            GTK_TEXT_VIEW(view),
                                            command,
                                            error_message);
            if (error)
            {
                xset_msg_dialog(GTK_WIDGET(dlg),
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Error Loading Handler",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                error_message);
                error_message.clear();
            }
        }
        free(xset_name);
    }
    else
    {
        // Recording the fact getting the iter failed
        LOG_WARN("Unable to fetch the iter from handler ordinal {}!", format);
    };

    // Mnemonically attaching widgets to labels
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_archive_format), combo);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_command), GTK_WIDGET(view));

    /* Creating hbox for the command textview, on a line under the top
     * hbox */
    GtkWidget* hbox_bottom = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox_bottom), GTK_WIDGET(view_scroll), true, true, 4);
    gtk_widget_show_all(hbox_bottom);

    // Packing the two hboxes into a vbox, then adding to dialog at bottom
    GtkWidget* vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox_top), true, true, 0);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox_bottom), true, true, 1);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), vbox, false, true, 0);

    // Configuring dialog
    gtk_file_chooser_set_action(GTK_FILE_CHOOSER(dlg),
                                GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), true);

    // Populating name of archive and setting the correct directory
    char* dest_file;
    if (!sel_files.empty())
    {
        // Fetching first extension handler deals with
        vfs::file_info file = sel_files.front();
        const std::string ext = archive_handler_get_first_extension(handler_xset);
        dest_file = g_strjoin(nullptr, file->get_disp_name().data(), ext.data(), nullptr);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), dest_file);
        free(dest_file);
        dest_file = nullptr;
    }
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), cwd.data());

    // Setting dimension and position
    const i32 width = xset_get_int(XSetName::ARC_DLG, XSetVar::X);
    const i32 height = xset_get_int(XSetName::ARC_DLG, XSetVar::Y);
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

    // Displaying dialog
    bool run_in_terminal;
    gtk_widget_show_all(dlg);

    bool exit_loop = false;
    i32 res;
    while ((res = gtk_dialog_run(GTK_DIALOG(dlg))))
    {
        switch (res)
        {
            case GtkResponseType::GTK_RESPONSE_OK:
                // Dialog OK'd - fetching archive filename
                dest_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));

                // Fetching archive handler selected
                if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
                {
                    // Unable to fetch iter from combo box - warning user and
                    // exiting
                    LOG_WARN("Unable to fetch iter from combobox!");
                    free(dest_file);
                    gtk_widget_destroy(dlg);
                    return;
                }

                // Fetching model data
                gtk_tree_model_get(GTK_TREE_MODEL(list),
                                   &iter,
                                   PTKFileArchiverCol::COL_XSET_NAME,
                                   &xset_name,
                                   // PTKFileArchiverExtensionsCol::COL_HANDLER_EXTENSIONS,
                                   // &extensions,
                                   -1);

                handler_xset = xset_get(xset_name);
                // Saving selected archive handler name as default
                xset_set(XSetName::ARC_DLG, XSetVar::S, xset_name);
                free(xset_name);

                // run in the terminal or not
                run_in_terminal = handler_xset->in_terminal;

                // Get command from text view
                GtkTextBuffer* buf;
                buf = gtk_text_view_get_buffer(view);
                GtkTextIter titer;
                GtkTextIter siter;
                gtk_text_buffer_get_start_iter(buf, &siter);
                gtk_text_buffer_get_end_iter(buf, &titer);
                command = gtk_text_buffer_get_text(buf, &siter, &titer, false);

                // reject command that contains only whitespace and comments
                if (ptk_handler_command_is_empty(command))
                {
                    xset_msg_dialog(
                        GTK_WIDGET(dlg),
                        GtkMessageType::GTK_MESSAGE_ERROR,
                        "Create Archive",
                        GtkButtonsType::GTK_BUTTONS_OK,
                        "The archive creation command is empty.  Please enter a command.");
                    continue;
                }
                // Getting prior command for comparison
                error = ptk_handler_load_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                                PtkHandlerArchive::HANDLER_COMPRESS,
                                                handler_xset,
                                                nullptr,
                                                compress_command,
                                                error_message);
                if (error)
                {
                    LOG_WARN(error_message);
                    error_message.clear();
                    compress_command = "";
                }

                // Checking to see if the compression command has changed
                if (!ztd::same(compress_command, command))
                {
                    // command has changed - saving command
                    if (handler_xset->disable)
                    {
                        // commmand was default - need to save all commands
                        // get default extract command from const
                        compress_command =
                            ptk_handler_get_command(PtkHandlerMode::HANDLER_MODE_ARC,
                                                    PtkHandlerArchive::HANDLER_EXTRACT,
                                                    handler_xset);
                        // write extract command script
                        error = ptk_handler_save_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                                        PtkHandlerArchive::HANDLER_EXTRACT,
                                                        handler_xset,
                                                        nullptr,
                                                        compress_command,
                                                        error_message);
                        if (error)
                        {
                            LOG_WARN(error_message);
                            error_message.clear();
                        }

                        // get default list command from const
                        compress_command = ptk_handler_get_command(PtkHandlerMode::HANDLER_MODE_ARC,
                                                                   PtkHandlerArchive::HANDLER_LIST,
                                                                   handler_xset);
                        // write list command script
                        error = ptk_handler_save_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                                        PtkHandlerArchive::HANDLER_LIST,
                                                        handler_xset,
                                                        nullptr,
                                                        compress_command,
                                                        error_message);
                        if (error)
                        {
                            LOG_WARN(error_message);
                            error_message.clear();
                        }

                        handler_xset->disable = false; // not default handler now
                    }
                    // save updated compress command
                    error = ptk_handler_save_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                                    PtkHandlerArchive::HANDLER_COMPRESS,
                                                    handler_xset,
                                                    nullptr,
                                                    command,
                                                    error_message);
                    if (error)
                    {
                        xset_msg_dialog(GTK_WIDGET(dlg),
                                        GtkMessageType::GTK_MESSAGE_ERROR,
                                        "Error Saving Handler",
                                        GtkButtonsType::GTK_BUTTONS_OK,
                                        error_message);
                        error_message.clear();
                    }
                }

                // Saving settings
                autosave_request_add();
                exit_loop = true;
                break;
            case GtkResponseType::GTK_RESPONSE_NONE:
                /* User wants to configure archive handlers - call up the
                 * config dialog then exit, as this dialog would need to be
                 * reconstructed if changes occur */
                gtk_widget_destroy(dlg);
                ptk_handler_show_config(PtkHandlerMode::HANDLER_MODE_ARC, file_browser, nullptr);
                return;
            default:
                // Destroying dialog
                gtk_widget_destroy(dlg);
                return;
        }
        if (exit_loop)
            break;
    }

    // Saving dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    const i32 new_width = allocation.width;
    const i32 new_height = allocation.height;
    if (new_width && new_height)
    {
        xset_set(XSetName::ARC_DLG, XSetVar::X, std::to_string(new_width));
        xset_set(XSetName::ARC_DLG, XSetVar::Y, std::to_string(new_height));
    }

    // Destroying dialog
    gtk_widget_destroy(dlg);

    // Make Archive Creation Command

    std::string desc;
    std::string ext;
    std::string udest_file;
    std::string udest_quote;
    std::string s1;
    std::string final_command;
    std::string cmd_to_run;

    // Dealing with separate archives for each source file/directory ('%O')
    if (ztd::contains(command, "%O"))
    {
        /* '%O' is present - the archiving command should be generated
         * and ran for each individual file */

        // Fetching extension
        ext = archive_handler_get_first_extension(handler_xset);

        /* Looping for all selected files/directories - all are used
         * when '%N' is present, only the first otherwise */
        i32 i = 0;
        const bool loop_once = ztd::contains(command, "%N");
        for (vfs::file_info file : sel_files)
        {
            desc = file->get_name();

            /* In %O mode, every source file is output to its own archive,
             * so the resulting archive name is based on the filename and
             * substituted every time */

            // Obtaining valid quoted UTF8 archive path to substitute for %O
            if (i == 0)
            {
                // First archive - use user-selected destination
                udest_file = Glib::filename_display_name(dest_file);
            }
            else
            {
                /* For subsequent archives, base archive name on the filename
                 * being compressed, in the user-selected dir */
                const std::string dest_dir = Glib::path_get_dirname(dest_file);
                udest_file = fmt::format("{}/{}{}", dest_dir, desc, ext);

                // Looping to find a path that doesnt exist
                i32 c = 1;
                while (std::filesystem::exists(udest_file))
                {
                    udest_file = fmt::format("{}/{}-{}{}{}", dest_dir, desc, "copy", ++c, ext);
                }
            }
            udest_quote = ztd::shell::quote(udest_file);

            /* Bash quoting desc - desc original value comes from the
             * VFSFileInfo struct and therefore should not be freed */
            if (desc.at(0) == '-')
            {
                // special handling for filename starting with a dash
                // due to tar interpreting it as option
                s1 = fmt::format("./{}", desc);
                desc = ztd::shell::quote(s1);
            }
            else
                desc = ztd::shell::quote(desc);

            // Replace sub vars  %n %N %O (and erroneous %o treat as %O)
            cmd_to_run = replace_archive_subs(command,
                                              i == 0 ? desc : "", // first run only %n = desc
                                              desc, // Replace %N with nth file (NOT ALL FILES)
                                              udest_quote,
                                              "",
                                              "");

            // Appending to final command as appropriate
            if (i == 0)
                final_command = fmt::format("{}\n[[ $? -eq 0 ]] || fm_handle_err\n", cmd_to_run);
            else
            {
                final_command = fmt::format("{}echo\n{}\n[[ $? -eq 0 ]] || fm_handle_err\n",
                                            final_command,
                                            cmd_to_run);
            }

            if (loop_once)
                break;
            ++i;
        }
    }
    else
    {
        /* '%O' is not present - the normal single command is needed
         * Obtaining valid quoted UTF8 file name %o for archive to create */
        udest_file = Glib::filename_display_name(dest_file);
        udest_quote = ztd::shell::quote(udest_file);
        std::string all;
        std::string first;
        if (!sel_files.empty())
        {
            desc = sel_files.front()->get_name();
            if (desc[0] == '-')
            {
                // special handling for filename starting with a dash
                // due to tar interpreting it as option
                s1 = fmt::format("./{}", desc);
                first = ztd::shell::quote(s1);
            }
            else
            {
                first = ztd::shell::quote(desc);
            }

            /* Generating string of selected files/directories to archive if
             * '%N' is present */
            if (ztd::contains(command, "%N"))
            {
                for (vfs::file_info file : sel_files)
                {
                    desc = file->get_name();
                    if (desc[0] == '-')
                    {
                        // special handling for filename starting with a dash
                        // due to tar interpreting it as option
                        s1 = fmt::format("./{}", desc);
                        desc = ztd::shell::quote(s1);
                    }
                    else
                    {
                        desc = ztd::shell::quote(desc);
                    }

                    all = fmt::format("{}{}{}", all, all.at(0) ? " " : "", desc);
                }
            }
        }
        else
        {
            // no files selected!
            first = ztd::strdup("");
        }

        // Replace sub vars  %n %N %o
        cmd_to_run = replace_archive_subs(command, first, all, udest_quote, "", "");

        // Enforce error check
        final_command = fmt::format("{}\n[[ $? -eq 0 ]] || fm_handle_err\n", cmd_to_run);
    }
    free(dest_file);

    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation - in any case an error check
     * needs to be made */
    const std::string str = generate_bash_error_function(run_in_terminal);
    final_command = fmt::format("{}\n\n{}", str, final_command);

    /* Cleaning up - final_command does not need freeing, as this
     * is freed by the task */

    // Creating task
    char* task_name = ztd::strdup("Archive");
    PtkFileTask* ptask = ptk_file_exec_new(task_name,
                                           cwd,
                                           file_browser ? GTK_WIDGET(file_browser) : nullptr,
                                           file_browser ? file_browser->task_view : nullptr);
    free(task_name);

    /* Setting correct exec reference - probably causes different bash
     * to be output */
    if (file_browser)
        ptask->task->exec_browser = file_browser;

    // Using terminals for certain handlers
    if (run_in_terminal)
    {
        ptask->task->exec_terminal = true;
        ptask->task->exec_sync = false;
    }
    else
        ptask->task->exec_sync = true;

    // Final configuration, setting custom icon
    ptask->task->exec_command = final_command;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = true; // Setup SpaceFM bash variables
    xset_t set = xset_get(XSetName::NEW_ARCHIVE);
    if (set->icon)
        ptask->task->exec_icon = set->icon;

    // Running task
    ptk_file_task_run(ptask);
}

static void
on_create_subfolder_toggled(GtkToggleButton* togglebutton, GtkWidget* chk_write)
{
    const bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));
    gtk_widget_set_sensitive(chk_write, enabled && geteuid() != 0);
}

void
ptk_file_archiver_extract(PtkFileBrowser* file_browser,
                          const std::vector<vfs::file_info>& sel_files, std::string_view cwd,
                          std::string_view dest_dir, i32 job, bool archive_presence_checked)
{ /* This function is also used to list the contents of archives */
    GtkWidget* dlgparent = nullptr;
    char* choose_dir = nullptr;
    bool create_parent = false;
    bool in_term = false;
    bool keep_term = false;
    bool write_access = false;
    bool list_contents = false;
    std::string parent_quote;
    const char* dest;
    std::string dest_quote;
    std::string full_quote;
    i32 res;

    // Making sure files to act on have been passed
    if (sel_files.empty() || job == PtkHandlerArchive::HANDLER_COMPRESS)
        return;

    /* Detecting whether this function call is actually to list the
     * contents of the archive or not... */
    list_contents = job == PtkHandlerArchive::HANDLER_LIST;

    /* Setting desired archive operation and keeping in terminal while
     * listing */
    const i32 archive_operation =
        list_contents ? PTKFileArchiverArc::ARC_LIST : PTKFileArchiverArc::ARC_EXTRACT;
    keep_term = list_contents;

    /* Ensuring archives are actually present in files, if this has not already
     * been verified - i.e. the function was triggered by a keyboard shortcut */
    if (!archive_presence_checked)
    {
        bool archive_found = false;

        // Looping for all files to attempt to list/extract
        for (vfs::file_info file : sel_files)
        {
            // Fetching file details
            vfs::mime_type mime_type = file->get_mime_type();
            const std::string full_path = Glib::build_filename(cwd.data(), file->get_name());

            // Checking for enabled handler with non-empty command
            const std::vector<xset_t> handlers =
                ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_ARC,
                                              archive_operation,
                                              full_path,
                                              mime_type,
                                              true,
                                              false,
                                              true);
            vfs_mime_type_unref(mime_type);
            if (!handlers.empty())
            {
                archive_found = true;
                break;
            }
        }

        if (!archive_found)
            return;
    }

    // Determining parent of dialog
    if (file_browser)
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser->main_window));

    // Checking if extract to directory has not been specified
    if (dest_dir.empty() && !list_contents)
    {
        /* It has not - generating dialog to ask user. Only dealing with
         * user-writable contents if the user is not root */
        GtkWidget* dlg =
            gtk_file_chooser_dialog_new("Extract To",
                                        dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                                        GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        nullptr,
                                        nullptr);

        /* Adding standard buttons and saving references in the dialog
         * 'Configure' button has custom text but a stock image */
        GtkButton* btn_configure =
            GTK_BUTTON(gtk_dialog_add_button(GTK_DIALOG(dlg),
                                             "Conf_igure",
                                             GtkResponseType::GTK_RESPONSE_NONE));
        g_object_set_data(G_OBJECT(dlg), "btn_configure", GTK_BUTTON(btn_configure));
        g_object_set_data(
            G_OBJECT(dlg),
            "btn_cancel",
            gtk_dialog_add_button(GTK_DIALOG(dlg), "Cancel", GtkResponseType::GTK_RESPONSE_CANCEL));
        g_object_set_data(
            G_OBJECT(dlg),
            "btn_ok",
            gtk_dialog_add_button(GTK_DIALOG(dlg), "OK", GtkResponseType::GTK_RESPONSE_OK));

        GtkWidget* hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget* chk_parent = gtk_check_button_new_with_mnemonic("Cre_ate subdirectories");
        GtkWidget* chk_write = gtk_check_button_new_with_mnemonic("Make contents "
                                                                  "user-_writable");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_parent), xset_get_b(XSetName::ARC_DLG));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_write),
                                     xset_get_int(XSetName::ARC_DLG, XSetVar::Z) == 1 &&
                                         geteuid() != 0);
        gtk_widget_set_sensitive(chk_write, xset_get_b(XSetName::ARC_DLG) && geteuid() != 0);
        g_signal_connect(G_OBJECT(chk_parent),
                         "toggled",
                         G_CALLBACK(on_create_subfolder_toggled),
                         chk_write);
        gtk_box_pack_start(GTK_BOX(hbox), chk_parent, false, false, 6);
        gtk_box_pack_start(GTK_BOX(hbox), chk_write, false, false, 6);
        gtk_widget_show_all(hbox);
        gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dlg), hbox);

        // Setting dialog to current working directory
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), cwd.data());

        // Fetching saved dialog dimensions and applying
        i32 width = xset_get_int(XSetName::ARC_DLG, XSetVar::X);
        i32 height = xset_get_int(XSetName::ARC_DLG, XSetVar::Y);
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

        // Displaying dialog
        bool exit_loop = false;
        while ((res = gtk_dialog_run(GTK_DIALOG(dlg))))
        {
            switch (res)
            {
                case GtkResponseType::GTK_RESPONSE_OK:
                    // Fetching user-specified settings and saving
                    choose_dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
                    create_parent = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_parent));
                    write_access =
                        create_parent && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_write));
                    xset_set_b(XSetName::ARC_DLG, create_parent);
                    xset_set(XSetName::ARC_DLG, XSetVar::Z, write_access ? "1" : "0");
                    exit_loop = true;
                    break;
                case GtkResponseType::GTK_RESPONSE_NONE:
                    /* User wants to configure archive handlers - call up the
                     * config dialog then exit, as this dialog would need to be
                     * reconstructed if changes occur */
                    gtk_widget_destroy(dlg);
                    ptk_handler_show_config(PtkHandlerMode::HANDLER_MODE_ARC,
                                            file_browser,
                                            nullptr);
                    return;
                default:
                    // Destroying dialog
                    gtk_widget_destroy(dlg);
                    return;
            }
            if (exit_loop)
                break;
        }

        // Saving dialog dimensions
        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
        width = allocation.width;
        height = allocation.height;
        if (width && height)
        {
            xset_set(XSetName::ARC_DLG, XSetVar::X, std::to_string(width));
            xset_set(XSetName::ARC_DLG, XSetVar::Y, std::to_string(height));
        }

        // Destroying dialog
        gtk_widget_destroy(dlg);

        // Exiting if user didnt choose an extraction directory
        if (!choose_dir)
            return;
        dest = choose_dir;
    }
    else
    {
        // Extraction directory specified - loading defaults
        create_parent = xset_get_b(XSetName::ARC_DEF_PARENT);
        write_access = create_parent && xset_get_b(XSetName::ARC_DEF_WRITE);

        dest = ztd::strdup(dest_dir.data());
    }

    /* Quoting destination directory (doing this outside of the later
     * loop as its needed after the selected files loop completes) */
    dest_quote = ztd::shell::quote(dest ? dest : cwd);

    // Fetching available archive handlers and splitting
    const std::string archive_handlers_s = xset_get_s(XSetName::ARC_CONF2);
    const std::vector<std::string> archive_handlers = ztd::split(archive_handlers_s, " ");

    xset_t handler_xset = nullptr;

    std::string command;
    std::string final_command;
    std::string error_message;
    // Looping for all files to attempt to list/extract
    for (vfs::file_info file : sel_files)
    {
        // Fetching file details
        vfs::mime_type mime_type = file->get_mime_type();
        // Determining file paths
        const std::string full_path = Glib::build_filename(cwd.data(), file->get_name());

        // Get handler with non-empty command
        const std::vector<xset_t> handlers =
            ptk_handler_file_has_handlers(PtkHandlerMode::HANDLER_MODE_ARC,
                                          archive_operation,
                                          full_path,
                                          mime_type,
                                          true,
                                          false,
                                          true);
        if (!handlers.empty())
            handler_xset = handlers.front();
        vfs_mime_type_unref(mime_type);

        // Continuing to next file if a handler hasnt been found
        if (!handler_xset)
        {
            LOG_WARN("No archive handler/command found for file: {}", full_path);
            continue;
        }
        LOG_INFO("Archive Handler Selected: {}", handler_xset->menu_label);

        /* Handler found - fetching the 'run in terminal' preference, if
         * the operation is listing then the terminal should be kept
         * open, otherwise the user should explicitly keep the terminal
         * running via the handler's command
         * Since multiple commands are now batched together, only one
         * of the handlers needing to run in a terminal will cause all of
         * them to */
        if (!in_term)
            in_term = archive_handler_run_in_term(handler_xset, archive_operation);

        // Archive to list or extract:
        full_quote = ztd::shell::quote(full_path); // %x
        std::string extract_target;                // %g or %G
        std::string mkparent;
        std::string perm;

        if (list_contents)
        {
            // List archive contents only
            const bool error = ptk_handler_load_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                                       PtkHandlerArchive::HANDLER_LIST,
                                                       handler_xset,
                                                       nullptr,
                                                       command,
                                                       error_message);
            if (error)
            {
                LOG_WARN(error_message);
                error_message.clear();
                command = "";
            }
        }
        else
        {
            /* An archive is to be extracted
             * Obtaining filename minus the archive extension - this is
             * needed if a parent directory must be created, and if the
             * extraction target is a file without the handler extension
             * filename is strdup'd to get rid of the const */
            const std::string filename = file->get_name();
            std::string filename_no_archive_ext;

            // Looping for all extensions registered with the current archive handler
            const std::vector<std::string> pathnames = ztd::split(handler_xset->x, " ");

            for (std::string_view pathname : pathnames)
            {
                // getting just the extension of the pathname list element
                const auto [filename_no_extension, filename_extension] =
                    get_name_extension(pathname);

                if (filename_extension.empty())
                    continue;

                // add a dot to extension
                const std::string new_extension = fmt::format(".{}", filename_extension);
                // Checking if the current extension is being used
                if (ztd::endswith(filename, new_extension))
                { // It is - determining filename without extension
                    filename_no_archive_ext = ztd::rpartition(filename, new_extension)[0];
                    break;
                }
            }

            /* An archive may not have an extension, or there may be no
             * extensions specified for the handler (they are optional)
             * - making sure filename_no_archive_ext is set in this case */
            if (filename_no_archive_ext.empty())
                filename_no_archive_ext = filename;

            /* Get extraction command - Doing this here as parent
             * directory creation needs access to the command. */
            const bool error = ptk_handler_load_script(PtkHandlerMode::HANDLER_MODE_ARC,
                                                       PtkHandlerArchive::HANDLER_EXTRACT,
                                                       handler_xset,
                                                       nullptr,
                                                       command,
                                                       error_message);
            if (error)
            {
                LOG_WARN(error_message);
                error_message.clear();
                command = "";
            }

            std::string parent_path;

            /* Dealing with creation of parent directory if needed -
             * never create a parent directory if '%G' is used - this is
             * an override substitution for the sake of gzip */
            if (create_parent && !ztd::contains(command, "%G"))
            {
                /* Determining full path of parent directory to make
                 * (also used later in '%g' substitution) */
                parent_path = Glib::build_filename(dest, filename_no_archive_ext);
                i32 n = 1;

                // Looping to find a path that doesnt exist
                while (std::filesystem::exists(parent_path))
                {
                    parent_path = fmt::format("{}-copy{}", parent_path, ++n);
                }

                // Generating shell command to make directory
                parent_quote = ztd::shell::quote(parent_path);
                mkparent = fmt::format("mkdir -p {} || fm_handle_err\n"
                                       "cd {} || fm_handle_err\n",
                                       parent_quote,
                                       parent_quote);

                /* Dealing with the need to make extracted files writable if
                 * desired (e.g. a tar of files originally archived from a CD
                 * will be readonly). Root users do not obey such access
                 * permissions and making such owned files writeable may be a
                 * security issue */
                if (write_access && geteuid() != 0)
                {
                    /* deliberately omitting fm_handle_error - only a
                     * convenience function */
                    perm = fmt::format("chmod -R u+rwX {}\n", parent_quote);
                }
                parent_quote.clear();
            }
            else
            {
                // Parent directory does not need to be created
                create_parent = false;
            }

            // Debug code
            // LOG_INFO("full_quote : {}", full_quote);
            // LOG_INFO("dest       : {}", dest);

            // Singular file extraction target (e.g. stdout-redirected gzip)
            static constexpr std::array<std::string_view, 2> keys{"%g", "%G"};
            if (ztd::contains(command, keys))
            {
                /* Creating extraction target, taking into account whether
                 * a parent directory has been created or not - target is
                 * guaranteed not to exist so as to avoid overwriting */
                extract_target = Glib::build_filename(create_parent ? parent_path : dest,
                                                      filename_no_archive_ext);

                /* Now the extraction filename is obtained, determine the
                 * normal filename without the extension */
                const auto [filename_no_extension, filename_extension] =
                    get_name_extension(filename_no_archive_ext);

                i32 n = 1;
                // Looping to find a path that doesnt exist
                while (std::filesystem::exists(extract_target))
                {
                    const std::string str2 =
                        fmt::format("{}-{}{}.{}", filename, "copy", ++n, filename_extension);
                    extract_target = Glib::build_filename(create_parent ? parent_path : dest, str2);
                }

                // Quoting target
                extract_target = ztd::shell::quote(extract_target);
            }
        }

        // Substituting %x %g %G
        command = replace_archive_subs(command, "", "", "", full_quote, extract_target);

        /* Finally constructing command to run, taking into account more than
         * one archive to list/extract. The mkparent command itself has error
         * checking - final error check not here as I want the code shared with
         * the list code flow */
        final_command = fmt::format("{}\ncd {} || fm_handle_err\n{}{}"
                                    "\n[[ $? -eq 0 ]] || fm_handle_err\n{}\n",
                                    final_command,
                                    dest_quote,
                                    mkparent,
                                    command,
                                    perm);
    }

    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation - in any case an error check
     * needs to be made */
    std::string str;
    if (create_parent)
        str = generate_bash_error_function(in_term, parent_quote);
    else
        str = generate_bash_error_function(in_term);
    final_command = fmt::format("{}\n{}", str, final_command);
    free(choose_dir);

    // Creating task
    const std::string task_name = fmt::format("Extract {}", sel_files.front()->get_name());
    PtkFileTask* ptask = ptk_file_exec_new(task_name,
                                           cwd,
                                           dlgparent,
                                           file_browser ? file_browser->task_view : nullptr);

    /* Setting correct exec reference - probably causes different bash
     * to be output */
    if (file_browser)
        ptask->task->exec_browser = file_browser;

    // Configuring task
    ptask->task->exec_command = final_command;
    ptask->task->exec_browser = file_browser;
    ptask->task->exec_sync = !in_term;
    ptask->task->exec_show_error = true;
    ptask->task->exec_scroll_lock = false;
    ptask->task->exec_show_output = list_contents && !in_term;
    ptask->task->exec_terminal = in_term;
    ptask->task->exec_keep_terminal = keep_term;
    ptask->task->exec_export = true; // Setup SpaceFM bash variables

    // Setting custom icon
    xset_t set = xset_get(XSetName::ARC_EXTRACT);
    if (set->icon)
        ptask->task->exec_icon = set->icon;

    // Running task
    ptk_file_task_run(ptask);
}
