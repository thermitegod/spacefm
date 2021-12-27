/*
 * SpaceFM ptk-file-archiver.c
 *
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 */

#include "ptk-file-archiver.hxx"
#include "ptk-file-task.hxx"
#include "ptk-handler.hxx"

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

static char*
archive_handler_get_first_extension(XSet* handler_xset)
{
    // Function deals with the possibility that a handler is responsible
    // for multiple MIME types and therefore file extensions. Functions
    // like archive creation need only one extension
    char* first_ext = nullptr;
    if (handler_xset && handler_xset->x)
    {
        // find first extension
        char** pathnames = g_strsplit(handler_xset->x, " ", -1);
        if (pathnames)
        {
            int i;
            for (i = 0; pathnames[i]; ++i)
            {
                // getting just the extension of the pathname list element
                char* name = get_name_extension(pathnames[i], false, &first_ext);
                g_free(name);
                if (first_ext)
                {
                    // add a dot to extension
                    char* str = first_ext;
                    first_ext = g_strconcat(".", first_ext, nullptr);
                    g_free(str);
                    break;
                }
            }
            g_strfreev(pathnames);
        }
    }
    if (first_ext)
        return first_ext;
    else
        return g_strdup("");
}

static bool
archive_handler_run_in_term(XSet* handler_xset, int operation)
{
    // Making sure a valid handler_xset has been passed
    if (!handler_xset)
    {
        g_warning("archive_handler_run_in_term has been called with an "
                  "invalid handler_xset!");
        return false;
    }

    int ret;
    switch (operation)
    {
        case ARC_COMPRESS:
            ret = handler_xset->in_terminal;
            break;
        case ARC_EXTRACT:
            ret = handler_xset->keep_terminal;
            break;
        case ARC_LIST:
            ret = handler_xset->scroll_lock;
            break;
        default:
            g_warning("archive_handler_run_in_term was passed an invalid"
                      " archive operation ('%d')!",
                      operation);
            return false;
    }
    return ret == XSET_B_TRUE;
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
    char* name = g_path_get_basename(path);
    g_free(path);

    // Fetching the combo model
    GtkListStore* list = (GtkListStore*)g_object_get_data(G_OBJECT(dlg), "combo-model");

    // Attempting to detect and remove extension from any current archive
    // handler - otherwise cycling through the handlers just appends
    // extensions to the filename
    // Obtaining iterator pointing at first handler
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter))
    {
        // Failed to get iterator - warning user and exiting
        g_warning("Unable to get an iterator to the start of the model "
                  "associated with combobox!");
        return;
    }

    // Loop through available handlers
    int len = 0;
    XSet* handler_xset;
    char* xset_name = nullptr;
    char* extension;
    do
    {
        gtk_tree_model_get(GTK_TREE_MODEL(list),
                           &iter,
                           COL_XSET_NAME,
                           &xset_name,
                           // COL_HANDLER_EXTENSIONS, &extensions,
                           -1);
        if ((handler_xset = xset_is(xset_name)))
        {
            // Obtaining archive extension
            extension = archive_handler_get_first_extension(handler_xset);

            // Checking to see if the current archive filename has this
            if (g_str_has_suffix(name, extension))
            {
                /* It does - recording its length if its the longest match
                 * yet, and continuing */
                if (strlen(extension) > len)
                    len = strlen(extension);
            }
            g_free(extension);
        }
        g_free(xset_name);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter));

    // Cropping current extension if found
    if (len)
    {
        len = strlen(name) - len;
        name[len] = '\0';
    }

    // Getting at currently selected archive handler
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
    {
        // You have to fetch both items here
        gtk_tree_model_get(GTK_TREE_MODEL(list),
                           &iter,
                           COL_XSET_NAME,
                           &xset_name,
                           // COL_HANDLER_EXTENSIONS, &extensions,
                           -1);
        if ((handler_xset = xset_is(xset_name)))
        {
            // Obtaining archive extension
            extension = archive_handler_get_first_extension(handler_xset);

            // Appending extension to original filename
            char* new_name = g_strconcat(name, extension, nullptr);

            // Updating new archive filename
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), new_name);

            g_free(new_name);
            g_free(extension);
        }
        g_free(xset_name);
    }

    g_free(name);

    // Loading command
    if (handler_xset)
    {
        GtkTextView* view = (GtkTextView*)g_object_get_data(G_OBJECT(dlg), "view");
        char* err_msg = ptk_handler_load_script(HANDLER_MODE_ARC,
                                                HANDLER_COMPRESS,
                                                handler_xset,
                                                GTK_TEXT_VIEW(view),
                                                nullptr);
        if (err_msg)
        {
            xset_msg_dialog(GTK_WIDGET(dlg),
                            GTK_MESSAGE_ERROR,
                            "Error Loading Handler",
                            0,
                            err_msg,
                            nullptr);
            g_free(err_msg);
        }
    }
}

static char*
generate_bash_error_function(bool run_in_terminal, char* parent_quote)
{
    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation. Even outside a terminal, IG
     * has requested text is output
     * No translation for security purposes */
    const char* error_pause = nullptr;
    const char* finished_with_errors = nullptr;
    if (run_in_terminal)
    {
        error_pause = g_strdup("read -p");
        finished_with_errors = g_strdup("[ Finished With Errors ]  Press Enter to close: ");
    }
    else
    {
        error_pause = g_strdup("echo");
        finished_with_errors = g_strdup("[ Finished With Errors ]");
    }

    return g_strdup_printf(""
                           "fm_handle_err(){\n"
                           "    fm_err=$?\n"
                           "%s%s%s"
                           "    if [ $fm_err -ne 0 ];then\n"
                           "       echo;%s \"%s\"\n"
                           "       exit $fm_err\n"
                           "    fi\n"
                           "}",
                           parent_quote ? "    rmdir --ignore-fail-on-non-empty " : "",
                           parent_quote ? parent_quote : "",
                           parent_quote ? "\n" : "",
                           error_pause,
                           finished_with_errors);
}

static char*
replace_archive_subs(const char* line, const char* n, const char* N, const char* o, const char* x,
                     const char* g)
{
    char* old_s;
    char* sub;
    char* percent;
    char ch;

    if (!line)
        return g_strdup("");

    char* s = g_strdup("");
    char* ptr = (char*)line;
    while (ptr[0])
    {
        percent = strchr(ptr, '%');
        if (!percent)
        {
            // no more percents - copy end of string
            old_s = s;
            s = g_strdup_printf("%s%s", s, ptr);
            g_free(old_s);
            break;
        }
        if (percent[1] == 'n' && n)
            sub = (char*)n;
        else if (percent[1] == 'N' && N)
            sub = (char*)N;
        else if ((percent[1] == 'o' || percent[1] == 'O') && o)
            sub = (char*)o;
        else if (percent[1] == 'x' && x)
            sub = (char*)x;
        else if ((percent[1] == 'g' || percent[1] == 'G') && g)
            sub = (char*)g;
        else if (percent[1] == '%')
        {
            // double percent %% - reduce to single and skip
            percent[1] = '\0';
            old_s = s;
            s = g_strdup_printf("%s%s", s, ptr);
            g_free(old_s);
            percent[1] = '%';
            ptr = percent + 2;
            continue;
        }
        else
        {
            // not recognized % - copy ptr to percent literally
            ch = percent[1]; // save the character after percent, change to null
            percent[1] = '\0';
            old_s = s;
            s = g_strdup_printf("%s%s", s, ptr);
            g_free(old_s);
            percent[1] = ch; // restore character after percent
            ptr = percent + 1;
            continue;
        }
        // copy ptr to percent - 1 and sub
        percent[0] = '\0'; // change % to end of string
        old_s = s;
        s = g_strdup_printf("%s%s%s", s, ptr, sub);
        g_free(old_s);
        percent[0] = '%'; // restore %
        ptr = percent + 2;
    }
    return s;
}

void
ptk_file_archiver_create(PtkFileBrowser* file_browser, GList* files, const char* cwd)
{
    /* Generating dialog - extra nullptr on the nullptr-terminated list to
     * placate an irrelevant compilation warning. See notes in
     * ptk-handler.c:ptk_handler_show_config about GTK failure to
     * identify top-level widget */
    GtkWidget* top_level =
        file_browser ? gtk_widget_get_toplevel(GTK_WIDGET(file_browser->main_window)) : nullptr;
    GtkWidget* dlg = gtk_file_chooser_dialog_new("Create Archive",
                                                 top_level ? GTK_WINDOW(top_level) : nullptr,
                                                 GTK_FILE_CHOOSER_ACTION_SAVE,
                                                 nullptr,
                                                 nullptr);

    /* Adding standard buttons and saving references in the dialog
     * 'Configure' button has custom text but a stock image */
    GtkButton* btn_configure =
        GTK_BUTTON(gtk_dialog_add_button(GTK_DIALOG(dlg), "Conf_igure", GTK_RESPONSE_NONE));
    g_object_set_data(G_OBJECT(dlg), "btn_configure", GTK_BUTTON(btn_configure));
    g_object_set_data(G_OBJECT(dlg),
                      "btn_cancel",
                      gtk_dialog_add_button(GTK_DIALOG(dlg), "Cancel", GTK_RESPONSE_CANCEL));
    g_object_set_data(G_OBJECT(dlg),
                      "btn_ok",
                      gtk_dialog_add_button(GTK_DIALOG(dlg), "OK", GTK_RESPONSE_OK));

    GtkFileFilter* filter = gtk_file_filter_new();

    /* Top hbox has 'Command:' label, 'Archive Format:' label then format
     * combobox */
    GtkWidget* hbox_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* lbl_command = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_command), "Co_mpress Commands:");
    gtk_box_pack_start(GTK_BOX(hbox_top), lbl_command, false, true, 2);

    // Generating a ComboBox with model behind, and saving model for use
    // in callback - now that archive handlers are custom, can't rely on
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
                                   COL_HANDLER_EXTENSIONS,
                                   nullptr);

    // Fetching available archive handlers
    char* archive_handlers_s = xset_get_s("arc_conf2");

    // Dealing with possibility of no handlers
    if (g_strcmp0(archive_handlers_s, "") <= 0)
    {
        /* Telling user to ensure handlers are available and bringing
         * up configuration */
        xset_msg_dialog(GTK_WIDGET(dlg),
                        GTK_MESSAGE_ERROR,
                        "Archive Handlers - Create Archive",
                        GTK_BUTTONS_OK,
                        "No archive handlers "
                        "configured. You must add a handler before "
                        "creating an archive.",
                        nullptr);
        ptk_handler_show_config(HANDLER_MODE_ARC, file_browser, nullptr);
        return;
    }

    // Splitting archive handlers
    char** archive_handlers = g_strsplit(archive_handlers_s, " ", -1);

    // Debug code
    // g_message("archive_handlers_s: %s", archive_handlers_s);

    // Looping for handlers (nullptr-terminated list)
    GtkTreeIter iter;
    char* extensions;
    XSet* handler_xset;
    // Get xset name of last used handler
    char* xset_name = xset_get_s("arc_dlg"); // do not free
    int format = 4;                          // default tar.gz
    int n = 0;
    int i;
    for (i = 0; archive_handlers[i] != nullptr; ++i)
    {
        // Fetching handler
        handler_xset = xset_is(archive_handlers[i]);

        if (handler_xset && handler_xset->b == XSET_B_TRUE)
        /* Checking to see if handler is enabled, can cope with
         * compression and the extension is set - dealing with empty
         * command yet 'run in terminal' still ticked
                               && handler_xset->y
                               && g_strcmp0( handler_xset->y, "" ) != 0
                               && g_strcmp0( handler_xset->y, "+" ) != 0
                               && g_strcmp0( handler_xset->x, "" ) != 0) */
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
            extensions = g_strconcat(handler_xset->menu_label, " (", handler_xset->x, ")", nullptr);
            gtk_list_store_set(GTK_LIST_STORE(list),
                               &iter,
                               COL_XSET_NAME,
                               archive_handlers[i],
                               COL_HANDLER_EXTENSIONS,
                               extensions,
                               -1);
            g_free(extensions);

            // Is last used handler?
            if (!g_strcmp0(xset_name, handler_xset->name))
                format = n;
            n++;
        }
    }

    // Clearing up archive_handlers
    g_strfreev(archive_handlers);

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
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    GtkWidget* view_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view_scroll), GTK_WIDGET(view));
    g_object_set_data(G_OBJECT(dlg), "view", view);

    /* Loading command for handler, based off the format handler */
    // Obtaining iterator from string turned into a path into the model
    char* str = g_strdup_printf("%d", format);
    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list), &iter, str))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(list),
                           &iter,
                           COL_XSET_NAME,
                           &xset_name,
                           // COL_HANDLER_EXTENSIONS, &extensions,
                           -1);
        if ((handler_xset = xset_is(xset_name)))
        {
            char* err_msg = ptk_handler_load_script(HANDLER_MODE_ARC,
                                                    HANDLER_COMPRESS,
                                                    handler_xset,
                                                    GTK_TEXT_VIEW(view),
                                                    nullptr);
            if (err_msg)
            {
                xset_msg_dialog(GTK_WIDGET(dlg),
                                GTK_MESSAGE_ERROR,
                                "Error Loading Handler",
                                0,
                                err_msg,
                                nullptr);
                g_free(err_msg);
            }
        }
        g_free(xset_name);
    }
    else
    {
        // Recording the fact getting the iter failed
        g_warning("Unable to fetch the iter from handler ordinal %d!", format);
    };
    g_free(str);

    // Mnemonically attaching widgets to labels
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_archive_format), combo);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_command), GTK_WIDGET(view));

    /* Creating hbox for the command textview, on a line under the top
     * hbox */
    GtkWidget* hbox_bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox_bottom), GTK_WIDGET(view_scroll), true, true, 4);
    gtk_widget_show_all(hbox_bottom);

    // Packing the two hboxes into a vbox, then adding to dialog at bottom
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox_top), true, true, 0);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox_bottom), true, true, 1);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), vbox, false, true, 0);

    // Configuring dialog
    gtk_file_chooser_set_action(GTK_FILE_CHOOSER(dlg), GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), true);

    // Populating name of archive and setting the correct directory
    char* dest_file;
    if (files)
    {
        // Fetching first extension handler deals with
        char* ext = archive_handler_get_first_extension(handler_xset);
        dest_file = g_strjoin(nullptr,
                              vfs_file_info_get_disp_name((VFSFileInfo*)files->data),
                              ext,
                              nullptr);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), dest_file);
        g_free(dest_file);
        dest_file = nullptr;
        g_free(ext);
        ext = nullptr;
    }
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), cwd);

    // Setting dimension and position
    int width = xset_get_int("arc_dlg", "x");
    int height = xset_get_int("arc_dlg", "y");
    if (width && height)
    {
        // filechooser won't honor default size or size request ?
        gtk_widget_show_all(dlg);
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_resize(GTK_WINDOW(dlg), width, height);
        while (gtk_events_pending())
            gtk_main_iteration();
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
    }

    // Displaying dialog
    char* command = nullptr;
    bool run_in_terminal;
    gtk_widget_show_all(dlg);

    bool exit_loop = false;
    int res;
    while ((res = gtk_dialog_run(GTK_DIALOG(dlg))))
    {
        switch (res)
        {
            case GTK_RESPONSE_OK:
                // Dialog OK'd - fetching archive filename
                dest_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));

                // Fetching archive handler selected
                if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
                {
                    // Unable to fetch iter from combo box - warning user and
                    // exiting
                    g_warning("Unable to fetch iter from combobox!");
                    g_free(dest_file);
                    gtk_widget_destroy(dlg);
                    return;
                }

                // Fetching model data
                gtk_tree_model_get(GTK_TREE_MODEL(list),
                                   &iter,
                                   COL_XSET_NAME,
                                   &xset_name,
                                   // COL_HANDLER_EXTENSIONS, &extensions,
                                   -1);

                handler_xset = xset_get(xset_name);
                // Saving selected archive handler name as default
                xset_set("arc_dlg", "s", xset_name);
                g_free(xset_name);

                // run in the terminal or not
                run_in_terminal = handler_xset->in_terminal == XSET_B_TRUE;

                // Get command from text view
                GtkTextBuffer* buf;
                buf = gtk_text_view_get_buffer(view);
                GtkTextIter iter;
                GtkTextIter siter;
                gtk_text_buffer_get_start_iter(buf, &siter);
                gtk_text_buffer_get_end_iter(buf, &iter);
                command = gtk_text_buffer_get_text(buf, &siter, &iter, false);

                // reject command that contains only whitespace and comments
                if (ptk_handler_command_is_empty(command))
                {
                    xset_msg_dialog(
                        GTK_WIDGET(dlg),
                        GTK_MESSAGE_ERROR,
                        "Create Archive",
                        0,
                        "The archive creation command is empty.  Please enter a command.",
                        nullptr);
                    g_free(command);
                    continue;
                }
                // Getting prior command for comparison
                char* err_msg;
                char* compress_cmd;
                compress_cmd = nullptr;
                err_msg = ptk_handler_load_script(HANDLER_MODE_ARC,
                                                  HANDLER_COMPRESS,
                                                  handler_xset,
                                                  nullptr,
                                                  &compress_cmd);
                if (err_msg)
                {
                    g_warning("%s", err_msg);
                    g_free(err_msg);
                    compress_cmd = g_strdup("");
                }

                // Checking to see if the compression command has changed
                if (g_strcmp0(compress_cmd, command))
                {
                    // command has changed - saving command
                    g_free(compress_cmd);
                    if (handler_xset->disable)
                    {
                        // commmand was default - need to save all commands
                        // get default extract command from const
                        compress_cmd = ptk_handler_get_command(HANDLER_MODE_ARC,
                                                               HANDLER_EXTRACT,
                                                               handler_xset);
                        // write extract command script
                        err_msg = ptk_handler_save_script(HANDLER_MODE_ARC,
                                                          HANDLER_EXTRACT,
                                                          handler_xset,
                                                          nullptr,
                                                          compress_cmd);
                        if (err_msg)
                        {
                            g_warning("%s", err_msg);
                            g_free(err_msg);
                        }
                        g_free(compress_cmd);
                        // get default list command from const
                        compress_cmd =
                            ptk_handler_get_command(HANDLER_MODE_ARC, HANDLER_LIST, handler_xset);
                        // write list command script
                        err_msg = ptk_handler_save_script(HANDLER_MODE_ARC,
                                                          HANDLER_LIST,
                                                          handler_xset,
                                                          nullptr,
                                                          compress_cmd);
                        if (err_msg)
                        {
                            g_warning("%s", err_msg);
                            g_free(err_msg);
                        }
                        g_free(compress_cmd);
                        handler_xset->disable = false; // not default handler now
                    }
                    // save updated compress command
                    err_msg = ptk_handler_save_script(HANDLER_MODE_ARC,
                                                      HANDLER_COMPRESS,
                                                      handler_xset,
                                                      nullptr,
                                                      command);
                    if (err_msg)
                    {
                        xset_msg_dialog(GTK_WIDGET(dlg),
                                        GTK_MESSAGE_ERROR,
                                        "Error Saving Handler",
                                        0,
                                        err_msg,
                                        nullptr);
                        g_free(err_msg);
                    }
                }
                else
                    g_free(compress_cmd);

                // Saving settings
                autosave_request();
                exit_loop = true;
                break;
            case GTK_RESPONSE_NONE:
                /* User wants to configure archive handlers - call up the
                 * config dialog then exit, as this dialog would need to be
                 * reconstructed if changes occur */
                gtk_widget_destroy(dlg);
                ptk_handler_show_config(HANDLER_MODE_ARC, file_browser, nullptr);
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
        str = g_strdup_printf("%d", width);
        xset_set("arc_dlg", "x", str);
        g_free(str);
        str = g_strdup_printf("%d", height);
        xset_set("arc_dlg", "y", str);
        g_free(str);
    }

    // Destroying dialog
    gtk_widget_destroy(dlg);

    // Make Archive Creation Command

    char* desc;
    char* ext;
    char* udest_file;
    char* udest_quote;
    char* s1;
    char* final_command;
    char* cmd_to_run;

    // Dealing with separate archives for each source file/directory ('%O')
    GList* l;
    if (g_strstr_len(command, -1, "%O"))
    {
        /* '%O' is present - the archiving command should be generated
         * and ran for each individual file */

        // Fetching extension
        ext = archive_handler_get_first_extension(handler_xset);

        /* Looping for all selected files/directories - all are used
         * when '%N' is present, only the first otherwise */
        for (i = 0, l = files; l && (i == 0 || g_strstr_len(command, -1, "%N")); l = l->next, ++i)
        {
            desc = (char*)vfs_file_info_get_name((VFSFileInfo*)l->data);

            /* In %O mode, every source file is output to its own archive,
             * so the resulting archive name is based on the filename and
             * substituted every time */

            // Obtaining valid quoted UTF8 archive path to substitute for %O
            if (i == 0)
            {
                // First archive - use user-selected destination
                udest_file = g_filename_display_name(dest_file);
            }
            else
            {
                /* For subsequent archives, base archive name on the filename
                 * being compressed, in the user-selected dir */
                char* dest_dir = g_path_get_dirname(dest_file);
                udest_file = g_strconcat(dest_dir, "/", desc, ext, nullptr);

                // Looping to find a path that doesnt exist
                struct stat statbuf;
                n = 1;
                while (lstat(udest_file, &statbuf) == 0)
                {
                    g_free(udest_file);
                    udest_file = g_strdup_printf("%s/%s-%s%d%s", dest_dir, desc, "copy", ++n, ext);
                }
                g_free(dest_dir);
            }
            udest_quote = bash_quote(udest_file);
            g_free(udest_file);
            udest_file = nullptr;

            /* Bash quoting desc - desc original value comes from the
             * VFSFileInfo struct and therefore should not be freed */
            if (desc[0] == '-')
            {
                // special handling for filename starting with a dash
                // due to tar interpreting it as option
                s1 = g_strdup_printf("./%s", desc);
                desc = bash_quote(s1);
                g_free(s1);
            }
            else
                desc = bash_quote(desc);

            // Replace sub vars  %n %N %O (and erroneous %o treat as %O)
            cmd_to_run = replace_archive_subs(command,
                                              i == 0 ? desc : "", // first run only %n = desc
                                              desc, // Replace %N with nth file (NOT ALL FILES)
                                              udest_quote,
                                              nullptr,
                                              nullptr);
            g_free(udest_quote);
            g_free(desc);

            // Appending to final command as appropriate
            if (i == 0)
                final_command =
                    g_strconcat(cmd_to_run, "\n[[ $? -eq 0 ]] || fm_handle_err\n", nullptr);
            else
            {
                s1 = final_command;
                final_command = g_strconcat(final_command,
                                            "echo\n",
                                            cmd_to_run,
                                            "\n[[ $? -eq 0 ]] || fm_handle_err\n",
                                            nullptr);
                g_free(s1);
            }
            g_free(cmd_to_run);
        }
    }
    else
    {
        /* '%O' isn't present - the normal single command is needed
         * Obtaining valid quoted UTF8 file name %o for archive to create */
        udest_file = g_filename_display_name(dest_file);
        udest_quote = bash_quote(udest_file);
        g_free(udest_file);
        char* all = g_strdup("");
        char* first;
        if (files)
        {
            desc = (char*)vfs_file_info_get_name((VFSFileInfo*)files->data);
            if (desc[0] == '-')
            {
                // special handling for filename starting with a dash
                // due to tar interpreting it as option
                s1 = g_strdup_printf("./%s", desc);
                first = bash_quote(s1);
                g_free(s1);
            }
            else
                first = bash_quote(desc);

            /* Generating string of selected files/directories to archive if
             * '%N' is present */
            if (g_strstr_len(command, -1, "%N"))
            {
                for (l = files; l; l = l->next)
                {
                    desc = (char*)vfs_file_info_get_name((VFSFileInfo*)l->data);
                    if (desc[0] == '-')
                    {
                        // special handling for filename starting with a dash
                        // due to tar interpreting it as option
                        s1 = g_strdup_printf("./%s", desc);
                        desc = bash_quote(s1);
                        g_free(s1);
                    }
                    else
                        desc = bash_quote(desc);

                    str = all;
                    all = g_strdup_printf("%s%s%s", all, all[0] ? " " : "", desc);
                    g_free(str);
                    g_free(desc);
                }
            }
        }
        else
        {
            // no files selected!
            first = g_strdup("");
        }

        // Replace sub vars  %n %N %o
        cmd_to_run = replace_archive_subs(command, first, all, udest_quote, nullptr, nullptr);

        // Enforce error check
        final_command = g_strconcat(cmd_to_run, "\n[[ $? -eq 0 ]] || fm_handle_err\n", nullptr);
        g_free(cmd_to_run);
        g_free(udest_quote);
        g_free(first);
        g_free(all);
    }
    g_free(dest_file);

    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation - in any case an error check
     * needs to be made */
    str = generate_bash_error_function(run_in_terminal, nullptr);
    s1 = final_command;
    final_command = g_strconcat(str, "\n\n", final_command, nullptr);
    g_free(str);
    g_free(s1);

    /* Cleaning up - final_command does not need freeing, as this
     * is freed by the task */
    g_free(command);

    // Creating task
    char* task_name = g_strdup_printf("Archive");
    PtkFileTask* task = ptk_file_exec_new(task_name,
                                          cwd,
                                          file_browser ? GTK_WIDGET(file_browser) : nullptr,
                                          file_browser ? file_browser->task_view : nullptr);
    g_free(task_name);

    /* Setting correct exec reference - probably causes different bash
     * to be output */
    if (file_browser)
        task->task->exec_browser = file_browser;

    // Using terminals for certain handlers
    if (run_in_terminal)
    {
        task->task->exec_terminal = true;
        task->task->exec_sync = false;
    }
    else
        task->task->exec_sync = true;

    // Final configuration, setting custom icon
    task->task->exec_command = final_command;
    task->task->exec_show_error = true;
    task->task->exec_export = true; // Setup SpaceFM bash variables
    XSet* set = xset_get("new_archive");
    if (set->icon)
        task->task->exec_icon = g_strdup(set->icon);

    // Running task
    ptk_file_task_run(task);
}

static void
on_create_subfolder_toggled(GtkToggleButton* togglebutton, GtkWidget* chk_write)
{
    bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));
    gtk_widget_set_sensitive(chk_write, enabled && geteuid() != 0);
}

void
ptk_file_archiver_extract(PtkFileBrowser* file_browser, GList* files, const char* cwd,
                          const char* dest_dir, int job, bool archive_presence_checked)
{ /* This function is also used to list the contents of archives */
    GtkWidget* dlgparent = nullptr;
    char* choose_dir = nullptr;
    bool create_parent = false;
    bool in_term = false;
    bool keep_term = false;
    bool write_access = false;
    bool list_contents = false;
    char* parent_quote = nullptr;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    const char* dest;
    GList* l;
    char* dest_quote = nullptr;
    char* full_path = nullptr;
    char* full_quote = nullptr;
    char* perm = nullptr;
    char* cmd = nullptr;
    char* str = nullptr;
    char* final_command = nullptr;
    char* s1 = nullptr;
    char* extension = nullptr;
    int i;
    int n;
    int res;
    struct stat statbuf;
    GSList* handlers_slist = nullptr;

    // Making sure files to act on have been passed
    if (!files || job == HANDLER_COMPRESS)
        return;

    /* Detecting whether this function call is actually to list the
     * contents of the archive or not... */
    list_contents = job == HANDLER_LIST;

    /* Setting desired archive operation and keeping in terminal while
     * listing */
    int archive_operation = list_contents ? ARC_LIST : ARC_EXTRACT;
    keep_term = list_contents;

    /* Ensuring archives are actually present in files, if this hasn't already
     * been verified - i.e. the function was triggered by a keyboard shortcut */
    if (!archive_presence_checked)
    {
        bool archive_found = false;

        // Looping for all files to attempt to list/extract
        for (l = files; l; l = l->next)
        {
            // Fetching file details
            file = (VFSFileInfo*)l->data;
            mime_type = vfs_file_info_get_mime_type(file);
            full_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);

            // Checking for enabled handler with non-empty command
            handlers_slist = ptk_handler_file_has_handlers(HANDLER_MODE_ARC,
                                                           archive_operation,
                                                           full_path,
                                                           mime_type,
                                                           true,
                                                           false,
                                                           true);
            g_free(full_path);
            vfs_mime_type_unref(mime_type);
            if (handlers_slist)
            {
                archive_found = true;
                g_slist_free(handlers_slist);
                break;
            }
        }

        if (!archive_found)
            return;
    }

    // Determining parent of dialog
    if (file_browser)
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser->main_window));

    // Checking if extract to directory hasn't been specified
    if (!dest_dir && !list_contents)
    {
        /* It hasn't - generating dialog to ask user. Only dealing with
         * user-writable contents if the user isn't root */
        GtkWidget* dlg = gtk_file_chooser_dialog_new("Extract To",
                                                     dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                                                     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                     nullptr,
                                                     nullptr);

        /* Adding standard buttons and saving references in the dialog
         * 'Configure' button has custom text but a stock image */
        GtkButton* btn_configure =
            GTK_BUTTON(gtk_dialog_add_button(GTK_DIALOG(dlg), "Conf_igure", GTK_RESPONSE_NONE));
        g_object_set_data(G_OBJECT(dlg), "btn_configure", GTK_BUTTON(btn_configure));
        g_object_set_data(G_OBJECT(dlg),
                          "btn_cancel",
                          gtk_dialog_add_button(GTK_DIALOG(dlg), "Cancel", GTK_RESPONSE_CANCEL));
        g_object_set_data(G_OBJECT(dlg),
                          "btn_ok",
                          gtk_dialog_add_button(GTK_DIALOG(dlg), "OK", GTK_RESPONSE_OK));

        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget* chk_parent = gtk_check_button_new_with_mnemonic("Cre_ate subdirectories");
        GtkWidget* chk_write = gtk_check_button_new_with_mnemonic("Make contents "
                                                                  "user-_writable");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_parent), xset_get_b("arc_dlg"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_write),
                                     xset_get_int("arc_dlg", "z") == 1 && geteuid() != 0);
        gtk_widget_set_sensitive(chk_write, xset_get_b("arc_dlg") && geteuid() != 0);
        g_signal_connect(G_OBJECT(chk_parent),
                         "toggled",
                         G_CALLBACK(on_create_subfolder_toggled),
                         chk_write);
        gtk_box_pack_start(GTK_BOX(hbox), chk_parent, false, false, 6);
        gtk_box_pack_start(GTK_BOX(hbox), chk_write, false, false, 6);
        gtk_widget_show_all(hbox);
        gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dlg), hbox);

        // Setting dialog to current working directory
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), cwd);

        // Fetching saved dialog dimensions and applying
        int width = xset_get_int("arc_dlg", "x");
        int height = xset_get_int("arc_dlg", "y");
        if (width && height)
        {
            // filechooser won't honor default size or size request ?
            gtk_widget_show_all(dlg);
            gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
            gtk_window_resize(GTK_WINDOW(dlg), width, height);
            while (gtk_events_pending())
                gtk_main_iteration();
            gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
        }

        // Displaying dialog
        bool exit_loop = false;
        while ((res = gtk_dialog_run(GTK_DIALOG(dlg))))
        {
            switch (res)
            {
                case GTK_RESPONSE_OK:
                    // Fetching user-specified settings and saving
                    choose_dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
                    create_parent = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_parent));
                    write_access =
                        create_parent && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_write));
                    xset_set_b("arc_dlg", create_parent);
                    xset_set("arc_dlg", "z", write_access ? "1" : "0");
                    exit_loop = true;
                    break;
                case GTK_RESPONSE_NONE:
                    /* User wants to configure archive handlers - call up the
                     * config dialog then exit, as this dialog would need to be
                     * reconstructed if changes occur */
                    gtk_widget_destroy(dlg);
                    ptk_handler_show_config(HANDLER_MODE_ARC, file_browser, nullptr);
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
            str = g_strdup_printf("%d", width);
            xset_set("arc_dlg", "x", str);
            g_free(str);
            str = g_strdup_printf("%d", height);
            xset_set("arc_dlg", "y", str);
            g_free(str);
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
        create_parent = xset_get_b("arc_def_parent");
        write_access = create_parent && xset_get_b("arc_def_write");

        dest = dest_dir;
    }

    /* Quoting destination directory (doing this outside of the later
     * loop as its needed after the selected files loop completes) */
    dest_quote = bash_quote(dest ? dest : cwd);

    // Fetching available archive handlers and splitting
    char* archive_handlers_s = xset_get_s("arc_conf2");
    char** archive_handlers =
        archive_handlers_s ? g_strsplit(archive_handlers_s, " ", -1) : nullptr;
    XSet* handler_xset = nullptr;

    // Looping for all files to attempt to list/extract
    for (l = files; l; l = l->next)
    {
        // Fetching file details
        file = (VFSFileInfo*)l->data;
        mime_type = vfs_file_info_get_mime_type(file);
        // Determining file paths
        full_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);

        // Get handler with non-empty command
        handlers_slist = ptk_handler_file_has_handlers(HANDLER_MODE_ARC,
                                                       archive_operation,
                                                       full_path,
                                                       mime_type,
                                                       true,
                                                       false,
                                                       true);
        if (handlers_slist)
        {
            handler_xset = (XSet*)handlers_slist->data;
            g_slist_free(handlers_slist);
        }
        else
            handler_xset = nullptr;
        vfs_mime_type_unref(mime_type);

        // Continuing to next file if a handler hasnt been found
        if (!handler_xset)
        {
            g_warning("%s %s", "No archive handler/command found for file:", full_path);
            g_free(full_path);
            continue;
        }
        printf("Archive Handler Selected: %s\n", handler_xset->menu_label);

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
        full_quote = bash_quote(full_path); // %x
        char* extract_target = nullptr;     // %g or %G
        char* mkparent = g_strdup("");
        perm = g_strdup("");

        if (list_contents)
        {
            // List archive contents only
            char* err_msg = ptk_handler_load_script(HANDLER_MODE_ARC,
                                                    HANDLER_LIST,
                                                    handler_xset,
                                                    nullptr,
                                                    &cmd);
            if (err_msg)
            {
                g_warning(err_msg, nullptr);
                g_free(err_msg);
                cmd = g_strdup("");
            }
        }
        else
        {
            /* An archive is to be extracted
             * Obtaining filename minus the archive extension - this is
             * needed if a parent directory must be created, and if the
             * extraction target is a file without the handler extension
             * filename is g_strdup'd to get rid of the const */
            char* filename = g_strdup(vfs_file_info_get_name(file));
            char* filename_no_archive_ext = nullptr;

            /* Looping for all extensions registered with the current
             * archive handler (nullptr-terminated list) */
            char** pathnames = handler_xset->x ? g_strsplit(handler_xset->x, " ", -1) : nullptr;
            char* filename_no_ext;
            if (pathnames)
            {
                for (i = 0; pathnames[i]; ++i)
                {
                    // getting just the extension of the pathname list element
                    filename_no_ext = get_name_extension(pathnames[i], false, &extension);
                    if (extension)
                    {
                        // add a dot to extension
                        str = extension;
                        extension = g_strconcat(".", extension, nullptr);
                        g_free(str);
                        // Checking if the current extension is being used
                        if (g_str_has_suffix(filename, extension))
                        {
                            // It is - determining filename without extension
                            n = strlen(filename) - strlen(extension);
                            char ch = filename[n];
                            filename[n] = '\0';
                            filename_no_archive_ext = g_strdup(filename);
                            filename[n] = ch;
                            break;
                        }
                    }
                    g_free(filename_no_ext);
                    g_free(extension);
                }
            }
            g_strfreev(pathnames);

            /* An archive may not have an extension, or there may be no
             * extensions specified for the handler (they are optional)
             * - making sure filename_no_archive_ext is set in this case */
            if (!filename_no_archive_ext)
                filename_no_archive_ext = g_strdup(filename);

            /* Now the extraction filename is obtained, determine the
             * normal filename without the extension */
            filename_no_ext = get_name_extension(filename_no_archive_ext, false, &extension);

            /* 'Completing' the extension and dealing with files with
             * no extension */
            if (!extension)
                extension = g_strdup("");
            else
            {
                str = extension;
                extension = g_strconcat(".", extension, nullptr);
                g_free(str);
            }

            /* Get extraction command - Doing this here as parent
             * directory creation needs access to the command. */
            char* err_msg;
            err_msg = ptk_handler_load_script(HANDLER_MODE_ARC,
                                              HANDLER_EXTRACT,
                                              handler_xset,
                                              nullptr,
                                              &cmd);
            if (err_msg)
            {
                g_warning(err_msg, nullptr);
                g_free(err_msg);
                cmd = g_strdup("");
            }

            /* Dealing with creation of parent directory if needed -
             * never create a parent directory if '%G' is used - this is
             * an override substitution for the sake of gzip */
            char* parent_path;
            parent_path = nullptr;
            if (create_parent && !g_strstr_len(cmd, -1, "%G"))
            {
                /* Determining full path of parent directory to make
                 * (also used later in '%g' substitution) */
                parent_path = g_build_filename(dest, filename_no_archive_ext, nullptr);
                char* parent_orig = g_strdup(parent_path);
                n = 1;

                // Looping to find a path that doesnt exist
                while (lstat(parent_path, &statbuf) == 0)
                {
                    g_free(parent_path);
                    parent_path = g_strdup_printf("%s-%s%d", parent_orig, "copy", ++n);
                }
                g_free(parent_orig);

                // Generating shell command to make directory
                parent_quote = bash_quote(parent_path);
                g_free(mkparent);
                mkparent = g_strdup_printf(""
                                           "mkdir -p %s || fm_handle_err\n"
                                           "cd %s || fm_handle_err\n",
                                           parent_quote,
                                           parent_quote);

                /* Dealing with the need to make extracted files writable if
                 * desired (e.g. a tar of files originally archived from a CD
                 * will be readonly). Root users don't obey such access
                 * permissions and making such owned files writeable may be a
                 * security issue */
                if (write_access && geteuid() != 0)
                {
                    /* deliberately omitting fm_handle_error - only a
                     * convenience function */
                    g_free(perm);
                    perm = g_strdup_printf("chmod -R u+rwX %s\n", parent_quote);
                }
                g_free(parent_quote);
                parent_quote = nullptr;
            }
            else
            {
                // Parent directory doesn't need to be created
                create_parent = false;
            }

            // Debug code
            // g_message( "full_quote: %s\ndest: %s", full_quote, dest );

            /* Singular file extraction target (e.g. stdout-redirected
             * gzip) */
            if (g_strstr_len(cmd, -1, "%g") || g_strstr_len(cmd, -1, "%G"))
            {
                /* Creating extraction target, taking into account whether
                 * a parent directory has been created or not - target is
                 * guaranteed not to exist so as to avoid overwriting */
                extract_target = g_build_filename(create_parent ? parent_path : dest,
                                                  filename_no_archive_ext,
                                                  nullptr);
                n = 1;

                // Looping to find a path that doesnt exist
                while (lstat(extract_target, &statbuf) == 0)
                {
                    g_free(extract_target);
                    str = g_strdup_printf("%s-%s%d%s", filename_no_ext, "copy", ++n, extension);
                    extract_target =
                        g_build_filename(create_parent ? parent_path : dest, str, nullptr);
                    g_free(str);
                }

                // Quoting target
                str = extract_target;
                extract_target = bash_quote(extract_target);
                g_free(str);
            }

            // Cleaning up
            g_free(filename);
            g_free(filename_no_archive_ext);
            g_free(filename_no_ext);
            g_free(extension);
            g_free(parent_path);
        }

        // Substituting %x %g %G
        str = cmd;
        cmd = replace_archive_subs(cmd, nullptr, nullptr, nullptr, full_quote, extract_target);
        g_free(str);

        /* Finally constructing command to run, taking into account more than
         * one archive to list/extract. The mkparent command itself has error
         * checking - final error check not here as I want the code shared with
         * the list code flow */
        str = final_command;
        final_command = g_strdup_printf("%s\ncd %s || fm_handle_err\n%s%s"
                                        "\n[[ $? -eq 0 ]] || fm_handle_err\n%s\n",
                                        (g_strcmp0(final_command, "") < 0) ? "" : final_command,
                                        dest_quote,
                                        mkparent,
                                        cmd,
                                        perm);
        g_free(str);

        // Cleaning up
        g_free(full_quote);
        g_free(full_path);
        g_free(cmd);
        g_free(mkparent);
        g_free(perm);
    }

    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation - in any case an error check
     * needs to be made */
    str = generate_bash_error_function(in_term, create_parent ? parent_quote : nullptr);
    s1 = final_command;
    final_command = g_strconcat(str, "\n", final_command, nullptr);
    g_free(str);
    g_free(s1);
    g_free(dest_quote);
    g_free(parent_quote);
    g_free(choose_dir);
    g_strfreev(archive_handlers);

    // Creating task
    char* task_name = g_strdup_printf("Extract %s", vfs_file_info_get_name(file));
    PtkFileTask* task = ptk_file_exec_new(task_name,
                                          cwd,
                                          dlgparent,
                                          file_browser ? file_browser->task_view : nullptr);
    g_free(task_name);

    /* Setting correct exec reference - probably causes different bash
     * to be output */
    if (file_browser)
        task->task->exec_browser = file_browser;

    // Configuring task
    task->task->exec_command = final_command;
    task->task->exec_browser = file_browser;
    task->task->exec_sync = !in_term;
    task->task->exec_show_error = true;
    task->task->exec_scroll_lock = false;
    task->task->exec_show_output = list_contents && !in_term;
    task->task->exec_terminal = in_term;
    task->task->exec_keep_terminal = keep_term;
    task->task->exec_export = true; // Setup SpaceFM bash variables

    // Setting custom icon
    XSet* set = xset_get("arc_extract");
    if (set->icon)
        task->task->exec_icon = g_strdup(set->icon);

    // Running task
    ptk_file_task_run(task);
}
