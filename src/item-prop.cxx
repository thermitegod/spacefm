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

#include <iostream>
#include <fstream>

#include <fmt/format.h>

#include <glibmm.h>

#include <gtk/gtk.h>

#include <fnmatch.h>

#include <exo/exo.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "item-prop.hxx"
#include "utils.hxx"

#include "ptk/ptk-app-chooser.hxx"

#include "settings.hxx"

const char* enter_command_use =
    "Enter program or bash command line(s):\n\nUse:\n\t%F\tselected files  or  %f first "
    "selected file\n\t%N\tselected filenames  or  %n first selected filename\n\t%d\tcurrent "
    "directory\n\t%v\tselected device (eg /dev/sda1)\n\t%m\tdevice mount point (eg /media/dvd); "
    " %l device label\n\t%b\tselected bookmark\n\t%t\tselected task directory;  %p task "
    "pid\n\t%a\tmenu item value\n\t$fm_panel, $fm_tab, etc";

enum ItemPropContextCol
{
    CONTEXT_COL_DISP,
    CONTEXT_COL_SUB,
    CONTEXT_COL_COMP,
    CONTEXT_COL_VALUE
};

enum ItemPropContextComp
{
    CONTEXT_COMP_EQUALS,
    CONTEXT_COMP_NEQUALS,
    CONTEXT_COMP_CONTAINS,
    CONTEXT_COMP_NCONTAINS,
    CONTEXT_COMP_BEGINS,
    CONTEXT_COMP_NBEGINS,
    CONTEXT_COMP_ENDS,
    CONTEXT_COMP_NENDS,
    CONTEXT_COMP_LESS,
    CONTEXT_COMP_GREATER,
    CONTEXT_COMP_MATCH,
    CONTEXT_COMP_NMATCH
};

enum ItemPropItemType
{
    ITEM_TYPE_BOOKMARK,
    ITEM_TYPE_APP,
    ITEM_TYPE_COMMAND
};

struct ContextData
{
    GtkWidget* dlg;
    GtkWidget* parent;
    GtkWidget* notebook;
    XSetContext* context;
    XSet* set;
    char* temp_cmd_line;
    struct stat script_stat;
    bool script_stat_valid;
    bool reset_command;

    // Menu Item Page
    GtkWidget* item_type;
    GtkWidget* item_name;
    GtkWidget* item_key;
    GtkWidget* item_icon;
    GtkWidget* target_vbox;
    GtkWidget* target_label;
    GtkWidget* item_target;
    GtkWidget* item_choose;
    GtkWidget* item_browse;
    GtkWidget* icon_choose_btn;

    // Context Page
    GtkWidget* vbox_context;
    GtkWidget* view;
    GtkButton* btn_remove;
    GtkButton* btn_add;
    GtkButton* btn_apply;
    GtkButton* btn_ok;

    GtkWidget* box_sub;
    GtkWidget* box_comp;
    GtkWidget* box_value;
    GtkWidget* box_match;
    GtkWidget* box_action;
    GtkLabel* current_value;
    GtkLabel* test;

    GtkWidget* hbox_match;
    GtkFrame* frame;
    GtkWidget* ignore_context;
    GtkWidget* hbox_opener;
    GtkWidget* opener;

    // Command Page
    GtkWidget* cmd_opt_line;
    GtkWidget* cmd_opt_script;
    GtkWidget* cmd_edit;
    GtkWidget* cmd_edit_root;
    GtkWidget* cmd_line_label;
    GtkWidget* cmd_scroll_script;
    GtkWidget* cmd_script;
    GtkWidget* cmd_opt_normal;
    GtkWidget* cmd_opt_checkbox;
    GtkWidget* cmd_opt_confirm;
    GtkWidget* cmd_opt_input;
    GtkWidget* cmd_vbox_msg;
    GtkWidget* cmd_scroll_msg;
    GtkWidget* cmd_msg;
    GtkWidget* opt_terminal;
    GtkWidget* opt_keep_term;
    GtkWidget* cmd_user;
    GtkWidget* opt_task;
    GtkWidget* opt_task_pop;
    GtkWidget* opt_task_err;
    GtkWidget* opt_task_out;
    GtkWidget* opt_scroll;
    GtkWidget* opt_hbox_task;
    GtkWidget* open_browser;
};

// clang-format off
static const std::array<const char*, 38> context_subs
{
    "MIME Type",
    "Filename",
    "Directory",
    "Dir Write Access",
    "File Is Text",
    "File Is Dir",
    "File Is Link",
    "User Is Root",
    "Multiple Selected",
    "Clipboard Has Files",
    "Clipboard Has Text",
    "Current Panel",
    "Panel Count",
    "Current Tab",
    "Tab Count",
    "Bookmark",
    "Device",
    "Device Mount Point",
    "Device Label",
    "Device FSType",
    "Device UDI",
    "Device Properties",
    "Task Count",
    "Task Directory",
    "Task Type",
    "Task Name",
    "Panel 1 Directory",
    "Panel 2 Directory",
    "Panel 3 Directory",
    "Panel 4 Directory",
    "Panel 1 Has Sel",
    "Panel 2 Has Sel",
    "Panel 3 Has Sel",
    "Panel 4 Has Sel",
    "Panel 1 Device",
    "Panel 2 Device",
    "Panel 3 Device",
    "Panel 4 Device",
};

static const std::array<const char*, 38> context_sub_lists
{
    "4%%%%%application/%%%%%audio/%%%%%audio/ || video/%%%%%image/%%%%%inode/directory%%%%%text/%%%%%video/%%%%%application/x-bzip||application/x-bzip-compressed-tar||application/x-gzip||application/zstd||application/x-lz4||application/zip||application/x-7z-compressed||application/x-bzip2||application/x-bzip2-compressed-tar||application/x-xz-compressed-tar||application/x-compressed-tar||application/x-rar",  //"MIME Type",
    "6%%%%%archive_types || .gz || .bz2 || .7z || .xz || .zst || .lz4 || .txz || .tgz || .tzst || .tlz4 || .zip || .rar || .tar || .tar.gz || .tar.xz || .tar.zst || .tar.lz4 || .tar.bz2 || .tar.7z%%%%%audio_types || .mp3 || .MP3 || .m3u || .wav || .wma || .aac || .ac3 || .opus || . flac || .ram || .m4a || .ogg%%%%%image_types || .jpg || .jpeg || .gif || .png || .xpm%%%%%video_types || .mp4 || .MP4 || .avi || .AVI || .mkv || .mpeg || .mpg || .flv || .vob || .asf || .rm || .m2ts || .mov",  //"Filename",
    "0%%%%%",  //"Dir",
    "0%%%%%false%%%%%true",  //"Dir Write Access",
    "0%%%%%false%%%%%true",  //"File Is Text",
    "0%%%%%false%%%%%true",  //"File Is Dir",
    "0%%%%%false%%%%%true",  //"File Is Link",
    "0%%%%%false%%%%%true",  //"User Is Root",
    "0%%%%%false%%%%%true",  //"Multiple Selected",
    "0%%%%%false%%%%%true",  //"Clipboard Has Files",
    "0%%%%%false%%%%%true",  //"Clipboard Has Text",
    "0%%%%%1%%%%%2%%%%%3%%%%%4",  //"Current Panel",
    "0%%%%%1%%%%%2%%%%%3%%%%%4",  //"Panel Count",
    "0%%%%%1%%%%%2%%%%%3%%%%%4%%%%%5%%%%%6",  //"Current Tab",
    "0%%%%%1%%%%%2%%%%%3%%%%%4%%%%%5%%%%%6",  //"Tab Count",
    "0%%%%%",  //"Bookmark",
    "0%%%%%/dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Device",
    "0%%%%%",  //"Device Mount Point",
    "0%%%%%",  //"Device Label",
    "0%%%%%ext2%%%%%ext3%%%%%ext4%%%%%ext2 || ext3 || ext4%%%%%ntfs%%%%%swap%%%%%ufs%%%%%vfat%%%%%xfs",  //Device FSType",
    "0%%%%%",  //"Device UDI",
    "2%%%%%audiocd%%%%%blank%%%%%dvd%%%%%dvd && blank%%%%%ejectable%%%%%floppy%%%%%internal%%%%%mountable%%%%%mounted%%%%%no_media%%%%%optical%%%%%optical && blank%%%%%optical && mountable%%%%%optical && mounted%%%%%removable%%%%%removable && mountable%%%%%removable && mounted%%%%%removable || optical%%%%%table%%%%%policy_hide%%%%%policy_noauto",  //"Device Properties",
    "8%%%%%0%%%%%1%%%%%2",  //"Task Count",
    "0%%%%%",  //"Task Dir",
    "0%%%%%change%%%%%copy%%%%%delete%%%%%link%%%%%move%%%%%run%%%%%trash",  //"Task Type",
    "0%%%%%",  //"Task Name",
    "0%%%%%",  //"Panel 1 Dir",
    "0%%%%%",  //"Panel 2 Dir",
    "0%%%%%",  //"Panel 3 Dir",
    "0%%%%%",  //"Panel 4 Dir",
    "0%%%%%false%%%%%true",  //"Panel 1 Has Sel",
    "0%%%%%false%%%%%true",  //"Panel 2 Has Sel",
    "0%%%%%false%%%%%true",  //"Panel 3 Has Sel",
    "0%%%%%false%%%%%true",  //"Panel 4 Has Sel",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Panel 1 Device",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Panel 2 Device",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Panel 3 Device",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0"  //"Panel 4 Device"
};

static const std::array<const char*, 12> context_comps
{
    "equals",
    "does not equal",
    "contains",
    "does not contain",
    "begins with",
    "does not begin with",
    "ends with",
    "does not end with",
    "is less than",
    "is greater than",
    "matches",
    "does not match",
};

static const std::array<const char*, 3> item_types
{
    "Bookmark",
    "Application",
    "Command",
};
// clang-format on

static char*
get_element_next(char** s)
{
    char* ret;

    if (!*s)
        return nullptr;
    char* sep = strstr(*s, "%%%%%");
    if (!sep)
    {
        if (*s[0] == '\0')
            return (*s = nullptr);
        ret = ztd::strdup(*s);
        *s = nullptr;
        return ret;
    }
    ret = strndup(*s, sep - *s);
    *s = sep + 5;
    return ret;
}

static bool
get_rule_next(char** s, int* sub, int* comp, char** value)
{
    char* vs;
    vs = get_element_next(s);
    if (!vs)
        return false;
    *sub = strtol(vs, nullptr, 10);
    free(vs);
    if (*sub < 0 || *sub >= (int)context_subs.size())
        return false;
    vs = get_element_next(s);
    *comp = strtol(vs, nullptr, 10);
    free(vs);
    if (*comp < 0 || *comp >= (int)context_comps.size())
        return false;
    if (!(*value = get_element_next(s)))
        *value = ztd::strdup("");
    return true;
}

int
xset_context_test(XSetContext* context, char* rules, bool def_disable)
{
    // assumes valid xset_context and rules != nullptr and no global ignore
    int i;
    int sep_type;
    int sub;
    int comp;
    char* value;
    int match, action;
    char* s;
    char* eleval;
    char* sep;
    bool test;

    enum ItemPropContextTest
    {
        ANY,
        ALL,
        NANY,
        NALL
    };

    // get valid action and match
    char* elements = rules;
    if (!(s = get_element_next(&elements)))
        return 0;
    action = strtol(s, nullptr, 10);
    free(s);
    if (action < 0 || action > 3)
        return 0;

    if (!(s = get_element_next(&elements)))
        return 0;
    match = strtol(s, nullptr, 10);
    free(s);
    if (match < 0 || match > 3)
        return 0;

    if (action != ItemPropContextState::CONTEXT_HIDE &&
        action != ItemPropContextState::CONTEXT_SHOW && def_disable)
        return ItemPropContextState::CONTEXT_DISABLE;

    // parse rules
    bool is_rules = false;
    bool all_match = true;
    bool no_match = true;
    bool any_match = false;
    while (get_rule_next(&elements, &sub, &comp, &value))
    {
        is_rules = true;

        eleval = value;
        do
        {
            if ((sep = strstr(eleval, "||")))
                sep_type = 1;
            else if ((sep = strstr(eleval, "&&")))
                sep_type = 2;

            if (sep)
            {
                sep[0] = '\0';
                i = -1;
                // remove trailing spaces from eleval
                while (sep + i >= eleval && sep[i] == ' ')
                {
                    sep[i] = '\0';
                    i--;
                }
            }

            switch (comp)
            {
                case ItemPropContextComp::CONTEXT_COMP_EQUALS:
                    test = !strcmp(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_NEQUALS:
                    test = strcmp(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_CONTAINS:
                    test = !!strstr(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_NCONTAINS:
                    test = !strstr(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_BEGINS:
                    test = Glib::str_has_prefix(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_NBEGINS:
                    test = !Glib::str_has_prefix(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_ENDS:
                    test = Glib::str_has_suffix(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_NENDS:
                    test = !Glib::str_has_suffix(context->var[sub], eleval);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_LESS:
                    test = strtol(context->var[sub], nullptr, 10) < strtol(eleval, nullptr, 10);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_GREATER:
                    test = strtol(context->var[sub], nullptr, 10) > strtol(eleval, nullptr, 10);
                    break;
                case ItemPropContextComp::CONTEXT_COMP_MATCH:
                case ItemPropContextComp::CONTEXT_COMP_NMATCH:
                    s = g_utf8_strdown(eleval, -1);
                    if (g_strcmp0(eleval, s))
                    {
                        // pattern contains uppercase chars - test case sensitive
                        test = fnmatch(eleval, context->var[sub], 0);
                    }
                    else
                    {
                        // case insensitive
                        char* str = g_utf8_strdown(context->var[sub], -1);
                        test = fnmatch(s, str, 0);
                        free(str);
                    }
                    free(s);
                    if (comp == ItemPropContextComp::CONTEXT_COMP_MATCH)
                        test = !test;
                    break;
                default:
                    test = match == ItemPropContextTest::NANY ||
                           match == ItemPropContextTest::NALL; // failsafe
            }

            if (sep)
            {
                if (test)
                {
                    if (sep_type == 1) // ||
                        break;
                }
                else
                {
                    if (sep_type == 2) // &&
                        break;
                }
                eleval = sep + 2;
                while (eleval[0] == ' ')
                    eleval++;
            }
            else
                eleval[0] = '\0';
        } while (eleval[0] != '\0');
        free(value);

        if (test)
        {
            any_match = true;
            no_match = false;
            if (match == ItemPropContextTest::ANY || match == ItemPropContextTest::NANY ||
                match == ItemPropContextTest::NALL)
                break;
        }
        else
        {
            all_match = false;
            if (match == ItemPropContextTest::ALL)
                break;
        }
    }

    if (!is_rules)
        return ItemPropContextState::CONTEXT_SHOW;

    bool is_match;
    switch (match)
    {
        case ItemPropContextTest::ALL:
            is_match = all_match;
            break;
        case ItemPropContextTest::NALL:
            is_match = !any_match;
            break;
        case ItemPropContextTest::NANY:
            is_match = no_match;
            break;
        case ItemPropContextTest::ANY:
            is_match = !no_match;
            break;
        default:
            break;
    }

    switch (action)
    {
        case ItemPropContextState::CONTEXT_SHOW:
            return is_match ? ItemPropContextState::CONTEXT_SHOW
                            : ItemPropContextState::CONTEXT_HIDE;
        case ItemPropContextState::CONTEXT_ENABLE:
            return is_match ? ItemPropContextState::CONTEXT_SHOW
                            : ItemPropContextState::CONTEXT_DISABLE;
        case ItemPropContextState::CONTEXT_DISABLE:
            return is_match ? ItemPropContextState::CONTEXT_DISABLE
                            : ItemPropContextState::CONTEXT_SHOW;
        default:
            break;
    }
    // ItemPropContextState::CONTEXT_HIDE
    if (is_match)
        return ItemPropContextState::CONTEXT_HIDE;
    return def_disable ? ItemPropContextState::CONTEXT_DISABLE : ItemPropContextState::CONTEXT_SHOW;
}

static char*
context_build(ContextData* ctxt)
{
    GtkTreeIter it;
    char* value;
    int sub, comp;
    std::string new_context;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        new_context = fmt::format("{}%%%%%{}",
                                  gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_action)),
                                  gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_match)));
        do
        {
            gtk_tree_model_get(model,
                               &it,
                               ItemPropContextCol::CONTEXT_COL_VALUE,
                               &value,
                               ItemPropContextCol::CONTEXT_COL_SUB,
                               &sub,
                               ItemPropContextCol::CONTEXT_COL_COMP,
                               &comp,
                               -1);
            new_context = fmt::format("{}%%%%%{}%%%%%{}%%%%%{}", new_context, sub, comp, value);
        } while (gtk_tree_model_iter_next(model, &it));
    }
    return ztd::strdup(new_context);
}

static void
enable_context(ContextData* ctxt)
{
    bool is_sel =
        gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view)),
                                        nullptr,
                                        nullptr);
    gtk_widget_set_sensitive(GTK_WIDGET(ctxt->btn_remove), is_sel);
    gtk_widget_set_sensitive(GTK_WIDGET(ctxt->btn_apply), is_sel);
    // gtk_widget_set_sensitive( GTK_WIDGET( ctxt->hbox_match ),
    //                    gtk_tree_model_get_iter_first(
    //                    gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) ), &it ) );
    if (ctxt->context && ctxt->context->valid)
    {
        char* rules = context_build(ctxt);
        std::string text = "Current: Show";
        if (rules)
        {
            int action = xset_context_test(ctxt->context, rules, false);
            if (action == ItemPropContextState::CONTEXT_HIDE)
                text = "Current: Hide";
            else if (action == ItemPropContextState::CONTEXT_DISABLE)
                text = "Current: Disable";
            else if (action == ItemPropContextState::CONTEXT_SHOW &&
                     gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_action)) ==
                         ItemPropContextState::CONTEXT_DISABLE)
                text = "Current: Enable";
        }
        gtk_label_set_text(ctxt->test, text.c_str());
    }
}

static void
on_context_action_changed(GtkComboBox* box, ContextData* ctxt)
{
    (void)box;
    enable_context(ctxt);
}

static char*
context_display(int sub, int comp, char* value)
{
    std::string disp;
    if (value[0] == '\0' || value[0] == ' ' || Glib::str_has_suffix(value, " "))
        disp = fmt::format("{} {} \"{}\"", context_subs[sub], context_comps[comp], value);
    else
        disp = fmt::format("{} {} {}", context_subs[sub], context_comps[comp], value);
    return ztd::strdup(disp);
}

static void
on_context_button_press(GtkWidget* widget, ContextData* ctxt)
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;

    if (widget == GTK_WIDGET(ctxt->btn_add) || widget == GTK_WIDGET(ctxt->btn_apply))
    {
        int sub = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_sub));
        int comp = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_comp));
        if (sub < 0 || comp < 0)
            return;
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
        if (widget == GTK_WIDGET(ctxt->btn_add))
            gtk_list_store_append(GTK_LIST_STORE(model), &it);
        else
        {
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view));
            if (!gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
                return;
        }
        char* value = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctxt->box_value));
        char* disp = context_display(sub, comp, value);
        gtk_list_store_set(GTK_LIST_STORE(model),
                           &it,
                           ItemPropContextCol::CONTEXT_COL_DISP,
                           disp,
                           ItemPropContextCol::CONTEXT_COL_SUB,
                           sub,
                           ItemPropContextCol::CONTEXT_COL_COMP,
                           comp,
                           ItemPropContextCol::CONTEXT_COL_VALUE,
                           value,
                           -1);
        free(disp);
        free(value);
        gtk_widget_set_sensitive(GTK_WIDGET(ctxt->btn_ok), true);
        if (widget == GTK_WIDGET(ctxt->btn_add))
            gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view)),
                                           &it);
        enable_context(ctxt);
        return;
    }

    // remove
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view));
    if (gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);

    enable_context(ctxt);
}

static void
on_context_sub_changed(GtkComboBox* box, ContextData* ctxt)
{
    (void)box;
    GtkTreeIter it;
    char* value;

    GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(ctxt->box_value));
    while (gtk_tree_model_get_iter_first(model, &it))
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);

    int sub = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_sub));
    if (sub < 0)
        return;
    char* elements = ztd::strdup(context_sub_lists[sub]);
    char* def_comp = get_element_next(&elements);
    if (def_comp)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_comp), strtol(def_comp, nullptr, 10));
        free(def_comp);
    }
    while ((value = get_element_next(&elements)))
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_value), value);
        free(value);
    }
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(ctxt->box_value))), "");
    if (ctxt->context && ctxt->context->valid)
        gtk_label_set_text(ctxt->current_value, ctxt->context->var[sub]);

    free(elements);
}

static void
on_context_row_activated(GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn* col,
                         ContextData* ctxt)
{
    (void)view;
    (void)col;
    GtkTreeIter it;
    char* value;
    int sub, comp;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
    if (!gtk_tree_model_get_iter(model, &it, tree_path))
        return;
    gtk_tree_model_get(model,
                       &it,
                       ItemPropContextCol::CONTEXT_COL_VALUE,
                       &value,
                       ItemPropContextCol::CONTEXT_COL_SUB,
                       &sub,
                       ItemPropContextCol::CONTEXT_COL_COMP,
                       &comp,
                       -1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_sub), sub);
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_comp), comp);
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(ctxt->box_value))), value);
    gtk_widget_grab_focus(ctxt->box_value);
    // enable_context( ctxt );
}

static bool
on_current_value_button_press(GtkWidget* widget, GdkEventButton* event, ContextData* ctxt)
{
    (void)widget;
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
        gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(ctxt->box_value))),
                           gtk_label_get_text(ctxt->current_value));
        gtk_widget_grab_focus(ctxt->box_value);
        return true;
    }
    return false;
}

static void
on_context_entry_insert(GtkEntryBuffer* buf, unsigned int position, char* chars,
                        unsigned int n_chars, void* user_data)
{ // remove linefeeds from pasted text
    (void)position;
    (void)chars;
    (void)n_chars;
    (void)user_data;
    if (!strchr(gtk_entry_buffer_get_text(buf), '\n'))
        return;

    std::string new_text = ztd::replace(gtk_entry_buffer_get_text(buf), "\n", "");
    gtk_entry_buffer_set_text(buf, new_text.c_str(), -1);
}

static bool
on_context_selection_change(GtkTreeSelection* tree_sel, ContextData* ctxt)
{
    (void)tree_sel;
    enable_context(ctxt);
    return false;
}

static bool
on_context_entry_keypress(GtkWidget* entry, GdkEventKey* event, ContextData* ctxt)
{
    (void)entry;
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
    {
        if (gtk_widget_get_sensitive(GTK_WIDGET(ctxt->btn_apply)))
            on_context_button_press(GTK_WIDGET(ctxt->btn_apply), ctxt);
        else
            on_context_button_press(GTK_WIDGET(ctxt->btn_add), ctxt);
        return true;
    }
    return false;
}

static void
enable_options(ContextData* ctxt)
{
    gtk_widget_set_sensitive(ctxt->opt_keep_term,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_terminal)));
    bool as_task = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task));
    gtk_widget_set_sensitive(ctxt->opt_task_pop, as_task);
    gtk_widget_set_sensitive(ctxt->opt_task_err, as_task);
    gtk_widget_set_sensitive(ctxt->opt_task_out, as_task);
    gtk_widget_set_sensitive(ctxt->opt_scroll, as_task);

    gtk_widget_set_sensitive(
        ctxt->cmd_vbox_msg,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_confirm)) ||
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_input)));
    gtk_widget_set_sensitive(
        ctxt->item_icon,
        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_checkbox)) &&
            ctxt->set->menu_style != XSetMenu::SEP && ctxt->set->menu_style != XSetMenu::SUBMENU);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_confirm)))
    {
        // add default msg
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_msg));
        if (gtk_text_buffer_get_char_count(buf) == 0)
            gtk_text_buffer_set_text(buf, "Are you sure?", -1);
    }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_input)))
    {
        // remove default msg
        GtkTextIter iter, siter;
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_msg));
        gtk_text_buffer_get_start_iter(buf, &siter);
        gtk_text_buffer_get_end_iter(buf, &iter);
        char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
        if (text && !strcmp(text, "Are you sure?"))
            gtk_text_buffer_set_text(buf, "", -1);
        free(text);
    }
}

static bool
is_command_script_newer(ContextData* ctxt)
{
    struct stat statbuf;

    if (!ctxt->script_stat_valid)
        return false;
    char* script = xset_custom_get_script(ctxt->set, false);
    if (script && stat(script, &statbuf) == 0)
    {
        if (statbuf.st_mtime != ctxt->script_stat.st_mtime ||
            statbuf.st_size != ctxt->script_stat.st_size)
            return true;
    }
    return false;
}

void
command_script_stat(ContextData* ctxt)
{
    char* script = xset_custom_get_script(ctxt->set, false);
    if (script && stat(script, &ctxt->script_stat) == 0)
        ctxt->script_stat_valid = true;
    else
        ctxt->script_stat_valid = false;
    free(script);
}

void
load_text_view(GtkTextView* view, const char* line)
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer(view);
    if (!line)
    {
        gtk_text_buffer_set_text(buf, "", -1);
        return;
    }
    std::string text = line;
    text = ztd::replace(text, "\\n", "\n");
    text = ztd::replace(text, "\\t", "\t");
    gtk_text_buffer_set_text(buf, text.c_str(), -1);
}

char*
get_text_view(GtkTextView* view)
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer(view);
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (!(text && text[0]))
    {
        free(text);
        return nullptr;
    }
    std::string text2 = text;
    text2 = ztd::replace(text2, "\\n", "\n");
    text2 = ztd::replace(text2, "\\t", "\t");

    return ztd::strdup(text2);
}

static void
load_command_script(ContextData* ctxt, XSet* set)
{
    bool modified = false;
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
    char* script = xset_custom_get_script(set, !set->plugin);
    gtk_text_buffer_set_text(buf, "", -1);
    if (script)
    {
        std::string line;
        std::ifstream file(script);
        if (!file.is_open())
        {
            std::string errno_msg = Glib::strerror(errno);
            LOG_WARN("error reading file {}: {}", script, errno_msg);
        }
        else
        {
            while (std::getline(file, line))
            {
                // read file one line at a time to prevent splitting UTF-8 characters
                if (!g_utf8_validate(line.c_str(), -1, nullptr))
                {
                    file.close();
                    gtk_text_buffer_set_text(buf, "", -1);
                    modified = true;
                    LOG_WARN("file '{}' contents are not valid UTF-8", script);
                    break;
                }
                gtk_text_buffer_insert_at_cursor(buf, line.c_str(), -1);
            }
            file.close();
        }
    }
    bool have_access = script && have_rw_access(script);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ctxt->cmd_script), !set->plugin && have_access);
    gtk_text_buffer_set_modified(buf, modified);
    command_script_stat(ctxt);
    free(script);
    if (have_access && geteuid() != 0)
        gtk_widget_hide(ctxt->cmd_edit_root);
    else
        gtk_widget_show(ctxt->cmd_edit_root);
}

static void
save_command_script(ContextData* ctxt, bool query)
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
    if (!gtk_text_buffer_get_modified(buf))
        return;
    if (query && xset_msg_dialog(ctxt->dlg,
                                 GTK_MESSAGE_QUESTION,
                                 "Save Modified Script?",
                                 GTK_BUTTONS_YES_NO,
                                 "Save your changes to the command script?") == GTK_RESPONSE_NO)
        return;
    if (is_command_script_newer(ctxt) &&
        xset_msg_dialog(
            ctxt->dlg,
            GTK_MESSAGE_QUESTION,
            "Overwrite Script?",
            GTK_BUTTONS_YES_NO,
            "The command script on disk has changed.\n\nDo you want to overwrite it?") ==
            GTK_RESPONSE_NO)
        return;

    char* script = xset_custom_get_script(ctxt->set, false);
    if (!script)
        return;

    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);

    std::ofstream file(script);
    if (file.is_open())
        file << text;

    file.close();

    free(text);
}

static void
on_script_toggled(GtkWidget* item, ContextData* ctxt)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        return;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_line)))
    {
        // set to command line
        save_command_script(ctxt, true);
        gtk_widget_show(ctxt->cmd_line_label);
        gtk_widget_show(ctxt->cmd_edit_root);
        load_text_view(GTK_TEXT_VIEW(ctxt->cmd_script), ctxt->temp_cmd_line);
    }
    else
    {
        // set to script
        gtk_widget_hide(ctxt->cmd_line_label);
        free(ctxt->temp_cmd_line);
        ctxt->temp_cmd_line = get_text_view(GTK_TEXT_VIEW(ctxt->cmd_script));
        load_command_script(ctxt, ctxt->set);
    }
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
    GtkTextIter siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_place_cursor(buf, &siter);
    gtk_widget_grab_focus(ctxt->cmd_script);
}

static void
on_cmd_opt_toggled(GtkWidget* item, ContextData* ctxt)
{
    enable_options(ctxt);
    if ((item == ctxt->cmd_opt_confirm || item == ctxt->cmd_opt_input) &&
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        gtk_widget_grab_focus(ctxt->cmd_msg);
    else if (item == ctxt->opt_terminal && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
        // checking run in terminal unchecks run as task
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_task), false);
}

static void
on_ignore_context_toggled(GtkWidget* item, ContextData* ctxt)
{
    gtk_widget_set_sensitive(ctxt->vbox_context,
                             !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)));
}

static void
on_edit_button_press(GtkWidget* btn, ContextData* ctxt)
{
    char* path;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_line)))
    {
        // set to command line - get path of first argument
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
        GtkTextIter iter, siter;
        gtk_text_buffer_get_start_iter(buf, &siter);
        gtk_text_buffer_get_end_iter(buf, &iter);
        char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
        if (!(text && text[0]))
            path = nullptr;
        else
        {
            char* str;
            if ((str = strchr(text, ' ')))
                str[0] = '\0';
            if ((str = strchr(text, '\n')))
                str[0] = '\0';
            path = ztd::strdup(g_strstrip(text));
            if (path[0] == '\0' || (path[0] != '/' && !g_ascii_isalnum(path[0])))
            {
                free(path);
                path = nullptr;
            }
            else if (path[0] != '/')
            {
                str = path;
                path = ztd::strdup(Glib::find_program_in_path(str));
                free(str);
            }
        }
        free(text);
        if (!(path && mime_type_is_text_file(path, nullptr)))
        {
            xset_msg_dialog(GTK_WIDGET(ctxt->dlg),
                            GTK_MESSAGE_ERROR,
                            "Error",
                            GTK_BUTTONS_OK,
                            "The command line does not begin with a text file (script) to be "
                            "opened, or the script was not found in your $PATH.");
            free(path);
            return;
        }
    }
    else
    {
        // set to script
        save_command_script(ctxt, false);
        path = xset_custom_get_script(ctxt->set, !ctxt->set->plugin);
    }
    if (path && mime_type_is_text_file(path, nullptr))
    {
        xset_edit(ctxt->dlg, path, btn == ctxt->cmd_edit_root, btn != ctxt->cmd_edit_root);
    }
    free(path);
}

static void
on_open_browser(GtkComboBox* box, ContextData* ctxt)
{
    char* folder = nullptr;
    int job = gtk_combo_box_get_active(GTK_COMBO_BOX(box));
    gtk_combo_box_set_active(GTK_COMBO_BOX(box), -1);
    if (job == 0)
    {
        // Command Dir
        if (ctxt->set->plugin)
        {
            folder = g_build_filename(ctxt->set->plug_dir, "files", nullptr);
            if (!std::filesystem::exists(folder))
            {
                free(folder);
                folder = g_build_filename(ctxt->set->plug_dir, ctxt->set->plug_name, nullptr);
            }
        }
        else
        {
            folder = g_build_filename(xset_get_config_dir(), "scripts", ctxt->set->name, nullptr);
        }
        if (!std::filesystem::exists(folder) && !ctxt->set->plugin)
        {
            std::filesystem::create_directories(folder);
            std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
        }
    }
    else if (job == 1)
    {
        // Data Dir
        if (ctxt->set->plugin)
        {
            XSet* mset = xset_get_plugin_mirror(ctxt->set);
            folder = g_build_filename(xset_get_config_dir(), "plugin-data", mset->name, nullptr);
        }
        else
            folder =
                g_build_filename(xset_get_config_dir(), "plugin-data", ctxt->set->name, nullptr);
        if (!std::filesystem::exists(folder))
        {
            std::filesystem::create_directories(folder);
            std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
        }
    }
    else if (job == 2)
    {
        // Plugin Dir
        if (ctxt->set->plugin && ctxt->set->plug_dir)
            folder = ztd::strdup(ctxt->set->plug_dir);
    }
    else
        return;
    if (folder && std::filesystem::is_directory(folder))
        open_in_prog(folder);
    free(folder);
}

static void
on_key_button_clicked(GtkWidget* widget, ContextData* ctxt)
{
    (void)widget;
    xset_set_key(ctxt->dlg, ctxt->set);

    XSet* keyset;
    if (ctxt->set->shared_key)
        keyset = xset_get(ctxt->set->shared_key);
    else
        keyset = ctxt->set;
    std::string str = xset_get_keyname(keyset, 0, 0);
    gtk_button_set_label(GTK_BUTTON(ctxt->item_key), str.c_str());
}

static void
on_type_changed(GtkComboBox* box, ContextData* ctxt)
{
    XSet* rset = ctxt->set;
    XSet* mset = xset_get_plugin_mirror(rset);
    int job = gtk_combo_box_get_active(GTK_COMBO_BOX(box));
    switch (job)
    {
        case ItemPropItemType::ITEM_TYPE_BOOKMARK:
        case ItemPropItemType::ITEM_TYPE_APP:
            // Bookmark or App
            gtk_widget_show(ctxt->target_vbox);
            gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 2));
            gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 3));

            if (job == ItemPropItemType::ITEM_TYPE_BOOKMARK)
            {
                gtk_widget_hide(ctxt->item_choose);
                gtk_widget_hide(ctxt->hbox_opener);
                gtk_label_set_text(GTK_LABEL(ctxt->target_label),
                                   "Targets:  (a semicolon-separated list of paths or URLs)");
            }
            else
            {
                gtk_widget_show(ctxt->item_choose);
                gtk_widget_show(ctxt->hbox_opener);
                gtk_label_set_text(GTK_LABEL(ctxt->target_label),
                                   "Target:  (a .desktop or executable file)");
            }
            break;
        case ItemPropItemType::ITEM_TYPE_COMMAND:
            // Command
            gtk_widget_hide(ctxt->target_vbox);
            gtk_widget_show(ctxt->hbox_opener);
            gtk_widget_show(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 2));
            gtk_widget_show(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 3));
            break;
        default:
            break;
    }

    // load command data
    if (rset->x && XSetCMD(strtol(rset->x, nullptr, 10)) == XSetCMD::SCRIPT)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_script), true);
        gtk_widget_hide(ctxt->cmd_line_label);
        load_command_script(ctxt, rset);
    }
    else
        load_text_view(GTK_TEXT_VIEW(ctxt->cmd_script), rset->line);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
    GtkTextIter siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_place_cursor(buf, &siter);

    // command options
    // if ctxt->reset_command is true, user may be switching from bookmark to
    // command, so reset the command options to defaults (they are not stored
    // for bookmarks/applications)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_terminal),
                                 mset->in_terminal && !ctxt->reset_command);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_keep_term),
                                 mset->keep_terminal || ctxt->reset_command);
    gtk_entry_set_text(GTK_ENTRY(ctxt->cmd_user), rset->y ? rset->y : "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_task),
                                 mset->task || ctxt->reset_command);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_pop),
                                 mset->task_pop && !ctxt->reset_command);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_err),
                                 mset->task_err || ctxt->reset_command);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_out),
                                 mset->task_out || ctxt->reset_command);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_scroll),
                                 !mset->scroll_lock || ctxt->reset_command);
    if (rset->menu_style == XSetMenu::CHECK)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_checkbox), true);
    else if (rset->menu_style == XSetMenu::CONFIRM)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_confirm), true);
    else if (rset->menu_style == XSetMenu::STRING)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_input), true);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_normal), true);
    load_text_view(GTK_TEXT_VIEW(ctxt->cmd_msg), rset->desc);
    enable_options(ctxt);
    if (geteuid() == 0)
    {
        // running as root
        gtk_widget_hide(ctxt->cmd_edit);
    }

    // TODO switch?
    if (job < ItemPropItemType::ITEM_TYPE_COMMAND)
    {
        // Bookmark or App
        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->item_target));
        gtk_text_buffer_set_text(buf, "", -1);
        gtk_entry_set_text(GTK_ENTRY(ctxt->item_name), "");
        gtk_entry_set_text(GTK_ENTRY(ctxt->item_icon), "");

        gtk_widget_grab_focus(ctxt->item_target);
        // click Browse
        // gtk_button_clicked( GTK_BUTTON( ctxt->item_browse ) );
    }

    if (job == ItemPropItemType::ITEM_TYPE_COMMAND || job == ItemPropItemType::ITEM_TYPE_APP)
    {
        // Opener
        if (mset->opener > 2 || mset->opener < 0)
            // forwards compat
            gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->opener), -1);
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->opener), mset->opener);
    }
}

static void
on_browse_button_clicked(GtkWidget* widget, ContextData* ctxt)
{
    int job = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->item_type));
    if (job == ItemPropItemType::ITEM_TYPE_BOOKMARK)
    {
        // Bookmark Browse
        char* add_path = xset_file_dialog(ctxt->dlg,
                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          "Choose Directory",
                                          ctxt->context->var[ItemPropContext::CONTEXT_DIR],
                                          nullptr);
        if (add_path && add_path[0])
        {
            char* old_path = multi_input_get_text(ctxt->item_target);
            std::string new_path = fmt::format("{}{}{}",
                                               old_path && old_path[0] ? old_path : "",
                                               old_path && old_path[0] ? "; " : "",
                                               add_path);
            GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->item_target));
            gtk_text_buffer_set_text(buf, new_path.c_str(), -1);
            free(add_path);
            free(old_path);
        }
    }
    else
    {
        // Application
        if (widget == ctxt->item_choose)
        {
            // Choose
            VFSMimeType* mime_type = vfs_mime_type_get_from_type(
                ctxt->context->var[ItemPropContext::CONTEXT_MIME] &&
                        ctxt->context->var[ItemPropContext::CONTEXT_MIME][0]
                    ? ctxt->context->var[ItemPropContext::CONTEXT_MIME]
                    : XDG_MIME_TYPE_UNKNOWN);
            char* app = (char*)ptk_choose_app_for_mime_type(
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(ctxt->dlg))),
                mime_type,
                true,
                false,
                false,
                false);
            if (app && app[0])
            {
                GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->item_target));
                gtk_text_buffer_set_text(buf, app, -1);
            }
            free(app);
            vfs_mime_type_unref(mime_type);
        }
        else
        {
            // Browse
            char* exec_path = xset_file_dialog(ctxt->dlg,
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               "Choose Executable",
                                               "/usr/bin",
                                               nullptr);
            if (exec_path && exec_path[0])
            {
                GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->item_target));
                gtk_text_buffer_set_text(buf, exec_path, -1);
                free(exec_path);
            }
        }
    }
}

static void
replace_item_props(ContextData* ctxt)
{
    XSetCMD x;
    XSet* rset = ctxt->set;
    XSet* mset = xset_get_plugin_mirror(rset);

    if (!rset->lock && rset->menu_style != XSetMenu::SUBMENU && rset->menu_style != XSetMenu::SEP &&
        rset->tool <= XSetTool::CUSTOM)
    {
        // custom bookmark, app, or command
        bool is_bookmark_or_app = false;
        int item_type = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->item_type));

        switch (item_type)
        {
            case ItemPropItemType::ITEM_TYPE_BOOKMARK:
                x = XSetCMD::BOOKMARK;
                is_bookmark_or_app = true;
                break;
            case ItemPropItemType::ITEM_TYPE_APP:
                x = XSetCMD::APP;
                is_bookmark_or_app = true;
                break;
            case ItemPropItemType::ITEM_TYPE_COMMAND:
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_line)))
                    // line
                    x = XSetCMD::LINE;
                else
                {
                    // script
                    x = XSetCMD::SCRIPT;
                    save_command_script(ctxt, false);
                }
                break;
            default:
                x = XSetCMD::INVALID;
                break;
        }

        if (x != XSetCMD::INVALID)
        {
            free(rset->x);
            if (x == XSetCMD::LINE)
                rset->x = nullptr;
            else
                rset->x = ztd::strdup(static_cast<int>(x));
        }
        if (!rset->plugin)
        {
            // target
            char* str = multi_input_get_text(ctxt->item_target);
            free(rset->z);
            rset->z = str ? g_strstrip(str) : nullptr;
            // run as user
            free(rset->y);
            rset->y = ztd::strdup(gtk_entry_get_text(GTK_ENTRY(ctxt->cmd_user)));
            // menu style
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_checkbox)))
                rset->menu_style = XSetMenu::CHECK;
            else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_confirm)))
                rset->menu_style = XSetMenu::CONFIRM;
            else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_input)))
                rset->menu_style = XSetMenu::STRING;
            else
                rset->menu_style = XSetMenu::NORMAL;
            // style msg
            free(rset->desc);
            rset->desc = get_text_view(GTK_TEXT_VIEW(ctxt->cmd_msg));
        }
        // command line
        free(rset->line);
        if (x == XSetCMD::LINE)
        {
            rset->line = get_text_view(GTK_TEXT_VIEW(ctxt->cmd_script));
            if (rset->line && std::strlen(rset->line) > 2000)
                xset_msg_dialog(ctxt->dlg,
                                GTK_MESSAGE_WARNING,
                                "Command Line Too Long",
                                GTK_BUTTONS_OK,
                                "Your command line is greater than 2000 characters and may be "
                                "truncated when saved.  Consider using a command script instead "
                                "by selecting Script on the Command tab.");
        }
        else
            rset->line = ztd::strdup(ctxt->temp_cmd_line);

        // run options
        mset->in_terminal = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_terminal)) &&
                            !is_bookmark_or_app;
        mset->keep_terminal =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_keep_term)) &&
            !is_bookmark_or_app;
        mset->task =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task)) && !is_bookmark_or_app;
        mset->task_pop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_pop)) &&
                         !is_bookmark_or_app;
        mset->task_err = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_err)) &&
                         !is_bookmark_or_app;
        mset->task_out = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_out)) &&
                         !is_bookmark_or_app;
        mset->scroll_lock =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_scroll)) || is_bookmark_or_app;

        // Opener
        if ((item_type == ItemPropItemType::ITEM_TYPE_COMMAND ||
             item_type == ItemPropItemType::ITEM_TYPE_APP))
        {
            if (gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->opener)) > -1)
                mset->opener = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->opener));
            // otherwise do not change for forward compat
        }
        else
            // reset if not applicable
            mset->opener = 0;
    }
    if (rset->menu_style != XSetMenu::SEP && !rset->plugin)
    {
        // name
        if (rset->lock &&
            g_strcmp0(rset->menu_label, gtk_entry_get_text(GTK_ENTRY(ctxt->item_name))))
            // built-in label has been changed from default, save it
            rset->in_terminal = true;

        free(rset->menu_label);
        if (rset->tool > XSetTool::CUSTOM &&
            !g_strcmp0(gtk_entry_get_text(GTK_ENTRY(ctxt->item_name)),
                       xset_get_builtin_toolitem_label(rset->tool)))
            // do not save default label of builtin toolitems
            rset->menu_label = nullptr;
        else
            rset->menu_label = ztd::strdup(gtk_entry_get_text(GTK_ENTRY(ctxt->item_name)));
    }
    // icon
    if (rset->menu_style != XSetMenu::RADIO && rset->menu_style != XSetMenu::SEP)
    // checkbox items in 1.0.1 allow icon due to bookmark list showing
    // toolbar checkbox items have icon
    //( rset->menu_style != XSetMenu::CHECK || rset->tool ) )
    {
        char* old_icon = ztd::strdup(mset->icon);
        free(mset->icon);
        const char* icon_name = gtk_entry_get_text(GTK_ENTRY(ctxt->item_icon));
        if (icon_name && icon_name[0])
            mset->icon = ztd::strdup(icon_name);
        else
            mset->icon = nullptr;

        if (rset->lock && g_strcmp0(old_icon, mset->icon))
            // built-in icon has been changed from default, save it
            rset->keep_terminal = true;
        free(old_icon);
    }

    // Ignore Context
    xset_set_b(XSetName::CONTEXT_DLG,
               gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->ignore_context)));
}

static void
on_script_popup(GtkTextView* input, GtkMenu* menu, void* user_data)
{
    (void)input;
    (void)user_data;
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set = xset_get(XSetName::SEPARATOR);
    set->menu_style = XSetMenu::SEP;
    set->browser = nullptr;
    xset_add_menuitem(nullptr, GTK_WIDGET(menu), accel_group, set);

    gtk_widget_show_all(GTK_WIDGET(menu));
}

static bool
delayed_focus(GtkWidget* widget)
{
    if (GTK_IS_WIDGET(widget))
        gtk_widget_grab_focus(widget);
    return false;
}

static void
on_prop_notebook_switch_page(GtkNotebook* notebook, GtkWidget* page, unsigned int page_num,
                             ContextData* ctxt)
{
    (void)notebook;
    (void)page;
    GtkWidget* widget;
    switch (page_num)
    {
        case 0:
            widget = ctxt->set->plugin ? ctxt->item_icon : ctxt->item_name;
            break;
        case 2:
            widget = ctxt->cmd_script;
            break;
        default:
            widget = nullptr;
            break;
    }
    g_idle_add((GSourceFunc)delayed_focus, widget);
}

static void
on_icon_choose_button_clicked(GtkWidget* widget, ContextData* ctxt)
{
    (void)widget;
    // get current icon
    char* new_icon;
    const char* icon = gtk_entry_get_text(GTK_ENTRY(ctxt->item_icon));

    new_icon = xset_icon_chooser_dialog(GTK_WINDOW(ctxt->dlg), icon);

    if (new_icon)
    {
        gtk_entry_set_text(GTK_ENTRY(ctxt->item_icon), new_icon);
        free(new_icon);
    }
}

static void
on_entry_activate(GtkWidget* entry, ContextData* ctxt)
{
    (void)entry;
    gtk_button_clicked(GTK_BUTTON(ctxt->btn_ok));
}

static bool
on_target_keypress(GtkWidget* widget, GdkEventKey* event, ContextData* ctxt)
{
    (void)widget;
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
    {
        gtk_button_clicked(GTK_BUTTON(ctxt->btn_ok));
        return true;
    }
    return false;
}

void
xset_item_prop_dlg(XSetContext* context, XSet* set, int page)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;

    if (!context || !set)
        return;
    ContextData* ctxt = g_slice_new0(ContextData);
    ctxt->context = context;
    ctxt->set = set;
    ctxt->script_stat_valid = ctxt->reset_command = false;
    ctxt->parent = nullptr;
    if (set->browser)
        ctxt->parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    // Dialog
    ctxt->dlg = gtk_dialog_new_with_buttons(
        set->tool != XSetTool::NOT ? "Toolbar Item Properties" : "Menu Item Properties",
        GTK_WINDOW(ctxt->parent),
        GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        nullptr,
        nullptr);
    xset_set_window_icon(GTK_WINDOW(ctxt->dlg));
    gtk_window_set_role(GTK_WINDOW(ctxt->dlg), "context_dialog");

    int width = xset_get_int(XSetName::CONTEXT_DLG, XSetSetSet::X);
    int height = xset_get_int(XSetName::CONTEXT_DLG, XSetSetSet::Y);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(ctxt->dlg), width, height);
    else
        gtk_window_set_default_size(GTK_WINDOW(ctxt->dlg), 800, 600);

    gtk_dialog_add_button(GTK_DIALOG(ctxt->dlg), "Cancel", GTK_RESPONSE_CANCEL);
    ctxt->btn_ok = GTK_BUTTON(gtk_dialog_add_button(GTK_DIALOG(ctxt->dlg), "OK", GTK_RESPONSE_OK));

    // Notebook
    ctxt->notebook = gtk_notebook_new();
    gtk_notebook_set_show_border(GTK_NOTEBOOK(ctxt->notebook), true);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(ctxt->notebook), true);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(ctxt->dlg))),
                       ctxt->notebook,
                       true,
                       true,
                       0);

    // Menu Item Page  =====================================================
    GtkWidget* align = gtk_alignment_new(0, 0, 0.4, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 0, 8, 8);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(align), vbox);
    gtk_notebook_append_page(
        GTK_NOTEBOOK(ctxt->notebook),
        align,
        gtk_label_new_with_mnemonic(set->tool != XSetTool::NOT ? "_Toolbar Item" : "_Menu Item"));

    GtkGrid* grid = GTK_GRID(gtk_grid_new());
    gtk_container_set_border_width(GTK_CONTAINER(grid), 0);
    gtk_grid_set_row_spacing(grid, 6);
    gtk_grid_set_column_spacing(grid, 8);
    int row = 0;

    GtkWidget* label = gtk_label_new_with_mnemonic("Type:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    ctxt->item_type = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_type), false);
    gtk_grid_attach(grid, ctxt->item_type, 1, row, 1, 1);

    label = gtk_label_new_with_mnemonic("Name:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
    row++;
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    ctxt->item_name = gtk_entry_new();
    gtk_grid_attach(grid, ctxt->item_name, 1, row, 1, 1);

    label = gtk_label_new_with_mnemonic("Key:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
    row++;
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    ctxt->item_key = gtk_button_new_with_label(" ");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_key), false);
    gtk_grid_attach(grid, ctxt->item_key, 1, row, 1, 1);

    label = gtk_label_new_with_mnemonic("Icon:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
    row++;
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    ctxt->icon_choose_btn = gtk_button_new_with_mnemonic("C_hoose");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->icon_choose_btn), false);

    // keep this
    gtk_button_set_always_show_image(GTK_BUTTON(ctxt->icon_choose_btn), true);
    ctxt->item_icon = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->item_icon), true, true, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->icon_choose_btn), false, true, 0);
    gtk_grid_attach(grid, hbox, 1, row, 1, 1);

    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(grid), false, true, 0);

    // Target
    ctxt->target_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    ctxt->target_label = gtk_label_new(nullptr);
    gtk_widget_set_halign(GTK_WIDGET(ctxt->target_label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ctxt->target_label), GTK_ALIGN_CENTER);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);
    ctxt->item_target = GTK_WIDGET(multi_input_new(GTK_SCROLLED_WINDOW(scroll), nullptr));
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->item_target), -1, 100);
    gtk_widget_set_size_request(GTK_WIDGET(scroll), -1, 100);

    gtk_box_pack_start(GTK_BOX(ctxt->target_vbox), GTK_WIDGET(ctxt->target_label), false, true, 0);
    gtk_box_pack_start(GTK_BOX(ctxt->target_vbox), GTK_WIDGET(scroll), false, true, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(gtk_label_new(nullptr)), true, true, 0);
    ctxt->item_choose = gtk_button_new_with_mnemonic("C_hoose");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_choose), false);

    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->item_choose), false, true, 12);

    ctxt->item_browse = gtk_button_new_with_mnemonic("_Browse");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_choose), false);

    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->item_browse), false, true, 0);

    gtk_box_pack_start(GTK_BOX(ctxt->target_vbox), GTK_WIDGET(hbox), false, true, 0);

    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->target_vbox), false, true, 4);

    align = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 0, 8, 8);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(align), vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctxt->notebook),
                             align,
                             gtk_label_new_with_mnemonic("Con_text"));

    GtkListStore* list =
        gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);

    // Listview
    ctxt->view = gtk_tree_view_new();
    gtk_tree_view_set_model(GTK_TREE_VIEW(ctxt->view), GTK_TREE_MODEL(list));
    // gtk_tree_view_set_model adds a ref
    g_object_unref(list);
    // gtk_tree_view_set_single_click(GTK_TREE_VIEW(ctxt->view), true);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ctxt->view), false);

    scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll), ctxt->view);
    g_signal_connect(G_OBJECT(ctxt->view),
                     "row-activated",
                     G_CALLBACK(on_context_row_activated),
                     ctxt);
    g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view))),
                     "changed",
                     G_CALLBACK(on_context_selection_change),
                     ctxt);

    // col display
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        ItemPropContextCol::CONTEXT_COL_DISP,
                                        nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctxt->view), col);
    gtk_tree_view_column_set_expand(col, true);

    // list buttons
    ctxt->btn_remove = GTK_BUTTON(gtk_button_new_with_mnemonic("_Remove"));
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->btn_remove), false);

    g_signal_connect(G_OBJECT(ctxt->btn_remove),
                     "clicked",
                     G_CALLBACK(on_context_button_press),
                     ctxt);

    ctxt->btn_add = GTK_BUTTON(gtk_button_new_with_mnemonic("_Add"));
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->btn_add), false);

    g_signal_connect(G_OBJECT(ctxt->btn_add), "clicked", G_CALLBACK(on_context_button_press), ctxt);

    ctxt->btn_apply = GTK_BUTTON(gtk_button_new_with_mnemonic("A_pply"));
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->btn_apply), false);

    g_signal_connect(G_OBJECT(ctxt->btn_apply),
                     "clicked",
                     G_CALLBACK(on_context_button_press),
                     ctxt);

    // boxes
    ctxt->box_sub = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->box_sub), false);
    for (const char* context_sub: context_subs)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_sub), context_sub);
    }
    g_signal_connect(G_OBJECT(ctxt->box_sub), "changed", G_CALLBACK(on_context_sub_changed), ctxt);

    ctxt->box_comp = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->box_comp), false);
    for (const char* context_comp: context_comps)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_comp), context_comp);
    }

    ctxt->box_value = gtk_combo_box_text_new_with_entry();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->box_value), false);

    // see https://github.com/IgnorantGuru/spacefm/issues/43
    // this seems to have no effect
    gtk_combo_box_set_popup_fixed_width(GTK_COMBO_BOX(ctxt->box_value), true);

    ctxt->box_match = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->box_match), false);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_match), "matches any rule:");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_match), "matches all rules:");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_match), "does not match any rule:");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_match),
                                   "does not match all rules:");
    g_signal_connect(G_OBJECT(ctxt->box_match),
                     "changed",
                     G_CALLBACK(on_context_action_changed),
                     ctxt);

    ctxt->box_action = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->box_action), false);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_action), "Show");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_action), "Enable");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_action), "Hide");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_action), "Disable");
    g_signal_connect(G_OBJECT(ctxt->box_action),
                     "changed",
                     G_CALLBACK(on_context_action_changed),
                     ctxt);

    ctxt->current_value = GTK_LABEL(gtk_label_new(nullptr));
    gtk_label_set_ellipsize(ctxt->current_value, PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(ctxt->current_value, true);
    gtk_widget_set_halign(GTK_WIDGET(ctxt->current_value), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ctxt->current_value), GTK_ALIGN_START);
    g_signal_connect(G_OBJECT(ctxt->current_value),
                     "button-press-event",
                     G_CALLBACK(on_current_value_button_press),
                     ctxt);
    g_signal_connect_after(
        G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(ctxt->box_value))))),
        "inserted-text",
        G_CALLBACK(on_context_entry_insert),
        nullptr);
    g_signal_connect(G_OBJECT(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(ctxt->box_value)))),
                     "key-press-event",
                     G_CALLBACK(on_context_entry_keypress),
                     ctxt);

    ctxt->test = GTK_LABEL(gtk_label_new(nullptr));

    // PACK
    gtk_container_set_border_width(GTK_CONTAINER(ctxt->dlg), 10);

    ctxt->vbox_context = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    ctxt->hbox_match = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_match), GTK_WIDGET(ctxt->box_action), false, true, 0);
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_match),
                       GTK_WIDGET(gtk_label_new("item if context")),
                       false,
                       true,
                       4);
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_match), GTK_WIDGET(ctxt->box_match), false, true, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(ctxt->hbox_match), false, true, 4);

    //    GtkLabel* label = gtk_label_new( "Rules:" );
    //    gtk_widget_set_halign(label, GTK_ALIGN_START);
    //    gtk_widget_set_valign(label, GTK_ALIGN_END);
    //    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( ctxt->dlg )->vbox ),
    //                        GTK_WIDGET( label ), false, true, 8 );
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(scroll), true, true, 4);

    GtkWidget* hbox_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->btn_remove), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns),
                       GTK_WIDGET(gtk_separator_new(GTK_ORIENTATION_VERTICAL)),
                       false,
                       true,
                       4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->btn_add), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->btn_apply), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->test), true, true, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(hbox_btns), false, true, 4);

    ctxt->frame = GTK_FRAME(gtk_frame_new("Edit Rule"));
    GtkWidget* vbox_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(ctxt->frame), vbox_frame);
    GtkWidget* hbox_frame = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox_frame), GTK_WIDGET(ctxt->box_sub), false, true, 8);
    gtk_box_pack_start(GTK_BOX(hbox_frame), GTK_WIDGET(ctxt->box_comp), false, true, 4);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox_frame), false, true, 4);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->box_value), true, true, 8);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), true, true, 4);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(gtk_label_new("Value:")), false, true, 8);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->current_value), true, true, 2);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), true, true, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(ctxt->frame), false, true, 16);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->vbox_context), true, true, 0);

    // Opener
    ctxt->hbox_opener = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_opener),
                       GTK_WIDGET(gtk_label_new("If enabled, use as handler for:")),
                       false,
                       true,
                       0);
    ctxt->opener = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->opener), false);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->opener), "none");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->opener), "files");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->opener), "devices");
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_opener), GTK_WIDGET(ctxt->opener), false, true, 4);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->hbox_opener), false, true, 0);

    // Ignore Context
    ctxt->ignore_context =
        gtk_check_button_new_with_mnemonic("_Ignore Context / Show All  (global setting)");
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->ignore_context), false, true, 0);
    if (xset_get_b(XSetName::CONTEXT_DLG))
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->ignore_context), true);

        gtk_widget_set_sensitive(ctxt->vbox_context, false);
    }
    g_signal_connect(G_OBJECT(ctxt->ignore_context),
                     "toggled",
                     G_CALLBACK(on_ignore_context_toggled),
                     ctxt);

    // plugin?
    XSet* mset; // mirror set or set
    XSet* rset; // real set
    if (set->plugin)
    {
        // set is plugin
        mset = xset_get_plugin_mirror(set);
        rset = set;
    }
    else if (!set->lock && set->desc && !strcmp(set->desc, "@plugin@mirror@") && set->shared_key)
    {
        // set is plugin mirror
        mset = set;
        rset = xset_get(set->shared_key);
        rset->browser = set->browser;
    }
    else
    {
        mset = set;
        rset = set;
    }
    ctxt->set = rset;

    // set match / action
    char* elements = mset->context;
    char* action = get_element_next(&elements);
    char* match = get_element_next(&elements);
    if (match && action)
    {
        int i = strtol(match, nullptr, 10);
        if (i < 0 || i > 3)
            i = 0;
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_match), i);
        i = strtol(action, nullptr, 10);
        if (i < 0 || i > 3)
            i = 0;
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_action), i);
        free(match);
        free(action);
    }
    else
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_match), 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_action), 0);
        if (match)
            free(match);
        if (action)
            free(action);
    }
    // set rules
    int sub, comp;
    char* value;
    char* disp;
    GtkTreeIter it;
    while (get_rule_next(&elements, &sub, &comp, &value))
    {
        disp = context_display(sub, comp, value);
        gtk_list_store_append(GTK_LIST_STORE(list), &it);
        gtk_list_store_set(GTK_LIST_STORE(list),
                           &it,
                           ItemPropContextCol::CONTEXT_COL_DISP,
                           disp,
                           ItemPropContextCol::CONTEXT_COL_SUB,
                           sub,
                           ItemPropContextCol::CONTEXT_COL_COMP,
                           comp,
                           ItemPropContextCol::CONTEXT_COL_VALUE,
                           value,
                           -1);
        free(disp);
        if (value)
            free(value);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_sub), 0);

    // Command Page  =====================================================
    align = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 0, 8, 8);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(align), vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctxt->notebook),
                             align,
                             gtk_label_new_with_mnemonic("Comm_and"));

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ctxt->cmd_opt_line = gtk_radio_button_new_with_mnemonic(nullptr, "Command _Line");
    ctxt->cmd_opt_script =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(ctxt->cmd_opt_line),
                                                       "_Script");
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_opt_line), false, true, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_opt_script), false, true, 0);
    ctxt->cmd_edit = gtk_button_new_with_mnemonic("Open In _Editor");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->cmd_edit), false);

    g_signal_connect(G_OBJECT(ctxt->cmd_edit), "clicked", G_CALLBACK(on_edit_button_press), ctxt);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_edit), false, true, 24);
    ctxt->cmd_edit_root = gtk_button_new_with_mnemonic("_Root Editor");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->cmd_edit_root), false);

    g_signal_connect(G_OBJECT(ctxt->cmd_edit_root),
                     "clicked",
                     G_CALLBACK(on_edit_button_press),
                     ctxt);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_edit_root), false, true, 24);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), false, true, 8);

    // Line
    ctxt->cmd_line_label = gtk_label_new(enter_command_use);
    gtk_widget_set_halign(GTK_WIDGET(ctxt->cmd_line_label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ctxt->cmd_line_label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->cmd_line_label), false, true, 8);

    // Script
    ctxt->cmd_scroll_script = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_script),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_script),
                                        GTK_SHADOW_ETCHED_IN);
    ctxt->cmd_script = GTK_WIDGET(gtk_text_view_new());
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_script), -1, 50);
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_scroll_script), -1, 50);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctxt->cmd_script), GTK_WRAP_WORD_CHAR);
    g_signal_connect_after(G_OBJECT(ctxt->cmd_script),
                           "populate-popup",
                           G_CALLBACK(on_script_popup),
                           nullptr);
    gtk_container_add(GTK_CONTAINER(ctxt->cmd_scroll_script), GTK_WIDGET(ctxt->cmd_script));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->cmd_scroll_script), true, true, 4);

    // Option Page  =====================================================
    align = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 0, 8, 8);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(align), vbox);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctxt->notebook),
                             align,
                             gtk_label_new_with_mnemonic("Optio_ns"));

    GtkWidget* frame = gtk_frame_new("Run Options");
    align = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 0, 8, 8);
    vbox_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(align), vbox_frame);
    gtk_container_add(GTK_CONTAINER(frame), align);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), false, true, 8);

    ctxt->opt_task = gtk_check_button_new_with_mnemonic("Run As Task");
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(ctxt->opt_task), false, true, 0);
    ctxt->opt_hbox_task = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ctxt->opt_task_pop = gtk_check_button_new_with_mnemonic("Popup Task");
    ctxt->opt_task_err = gtk_check_button_new_with_mnemonic("Popup Error");
    ctxt->opt_task_out = gtk_check_button_new_with_mnemonic("Popup Output");
    ctxt->opt_scroll = gtk_check_button_new_with_mnemonic("Scroll Output");
    gtk_box_pack_start(GTK_BOX(ctxt->opt_hbox_task),
                       GTK_WIDGET(ctxt->opt_task_pop),
                       false,
                       true,
                       0);
    gtk_box_pack_start(GTK_BOX(ctxt->opt_hbox_task),
                       GTK_WIDGET(ctxt->opt_task_err),
                       false,
                       true,
                       6);
    gtk_box_pack_start(GTK_BOX(ctxt->opt_hbox_task),
                       GTK_WIDGET(ctxt->opt_task_out),
                       false,
                       true,
                       6);
    gtk_box_pack_start(GTK_BOX(ctxt->opt_hbox_task), GTK_WIDGET(ctxt->opt_scroll), false, true, 6);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(ctxt->opt_hbox_task), false, true, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ctxt->opt_terminal = gtk_check_button_new_with_mnemonic("Run In Terminal");
    ctxt->opt_keep_term = gtk_check_button_new_with_mnemonic("Keep Terminal Open");
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->opt_terminal), false, true, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->opt_keep_term), false, true, 6);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), false, true, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    label = gtk_label_new("Run As User:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), false, true, 2);
    ctxt->cmd_user = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_user), false, true, 8);
    label = gtk_label_new("( leave blank for current user )");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), false, true, 8);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), false, true, 4);

    frame = gtk_frame_new("Style");
    align = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 0, 8, 8);
    vbox_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(align), vbox_frame);
    gtk_container_add(GTK_CONTAINER(frame), align);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), true, true, 8);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ctxt->cmd_opt_normal = gtk_radio_button_new_with_mnemonic(nullptr, "Normal");
    ctxt->cmd_opt_checkbox =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(ctxt->cmd_opt_normal),
                                                       "Checkbox");
    ctxt->cmd_opt_confirm =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(ctxt->cmd_opt_normal),
                                                       "Confirmation");
    ctxt->cmd_opt_input =
        gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(ctxt->cmd_opt_normal),
                                                       "Input");
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_opt_normal), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_opt_checkbox), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_opt_confirm), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_opt_input), false, true, 4);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), false, true, 0);

    // message box
    ctxt->cmd_vbox_msg = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    label = gtk_label_new("Confirmation/Input Message:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(ctxt->cmd_vbox_msg), GTK_WIDGET(label), false, true, 8);
    ctxt->cmd_scroll_msg = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_msg),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_msg),
                                        GTK_SHADOW_ETCHED_IN);
    ctxt->cmd_msg = GTK_WIDGET(gtk_text_view_new());
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_msg), -1, 50);
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_scroll_msg), -1, 50);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctxt->cmd_msg), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(ctxt->cmd_scroll_msg), GTK_WIDGET(ctxt->cmd_msg));
    gtk_box_pack_start(GTK_BOX(ctxt->cmd_vbox_msg),
                       GTK_WIDGET(ctxt->cmd_scroll_msg),
                       true,
                       true,
                       4);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(ctxt->cmd_vbox_msg), true, true, 0);

    // open directory
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    label = gtk_label_new("Open In Browser:");
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), false, true, 0);
    ctxt->open_browser = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->open_browser), false);

    std::string str;
    char* path;
    if (rset->plugin)
        path = g_build_filename(rset->plug_dir, rset->plug_name, nullptr);
    else
        path = g_build_filename(xset_get_config_dir(), "scripts", rset->name, nullptr);
    str = fmt::format("Command Dir  $fm_cmd_dir  {}", dir_has_files(path) ? "" : "(no files)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->open_browser), str.c_str());
    free(path);

    path = g_build_filename(xset_get_config_dir(),
                            "plugin-data",
                            rset->plugin ? mset->name : rset->name,
                            nullptr);
    str = fmt::format("Data Dir  $fm_cmd_data  {}", dir_has_files(path) ? "" : "(no files)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->open_browser), str.c_str());
    free(path);

    if (rset->plugin)
    {
        str = "Plugin Dir  $fm_plugin_dir";
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->open_browser), str.c_str());
    }
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->open_browser), false, true, 8);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), false, true, 0);

    // show all
    gtk_widget_show_all(GTK_WIDGET(ctxt->dlg));

    // load values  ========================================================
    // type
    int item_type = -1;

    std::string item_type_str;
    if (set->tool > XSetTool::CUSTOM)
        item_type_str =
            fmt::format("Built-In Toolbar Item: {}", xset_get_builtin_toolitem_label(set->tool));
    else if (rset->menu_style == XSetMenu::SUBMENU)
        item_type_str = "Submenu";
    else if (rset->menu_style == XSetMenu::SEP)
        item_type_str = "Separator";
    else if (set->lock)
    {
        // built-in
        item_type_str = "Built-In Command";
    }
    else
    {
        // custom command
        XSetCMD x;
        for (const char* item_type2: item_types)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->item_type), item_type2);
        }
        x = rset->x ? XSetCMD(strtol(rset->x, nullptr, 10)) : XSetCMD::LINE;

        switch (x)
        {
            case XSetCMD::LINE:
            case XSetCMD::SCRIPT:
                item_type = ItemPropItemType::ITEM_TYPE_COMMAND;
                break;
            case XSetCMD::APP:
                item_type = ItemPropItemType::ITEM_TYPE_APP;
                break;
            case XSetCMD::BOOKMARK:
                item_type = ItemPropItemType::ITEM_TYPE_BOOKMARK;
                break;
            case XSetCMD::INVALID:
            default:
                item_type = -1;
                break;
        }

        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->item_type), item_type);
        // g_signal_connect( G_OBJECT( ctxt->item_type ), "changed",
        //                  G_CALLBACK( on_item_type_changed ), ctxt );
    }
    if (!item_type_str.empty())
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->item_type), item_type_str.c_str());
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->item_type), 0);
        gtk_widget_set_sensitive(ctxt->item_type, false);
    }

    ctxt->temp_cmd_line = !set->lock ? ztd::strdup(rset->line) : nullptr;
    if (set->lock || rset->menu_style == XSetMenu::SUBMENU || rset->menu_style == XSetMenu::SEP ||
        set->tool > XSetTool::CUSTOM)
    {
        gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 2));
        gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 3));
        gtk_widget_hide(ctxt->target_vbox);
        gtk_widget_hide(ctxt->hbox_opener);
    }
    else
    {
        // load command values
        on_type_changed(GTK_COMBO_BOX(ctxt->item_type), ctxt);
        if (rset->z)
        {
            GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->item_target));
            gtk_text_buffer_set_text(buf, rset->z, -1);
        }
    }
    ctxt->reset_command = true;

    // name
    if (rset->menu_style != XSetMenu::SEP)
    {
        if (set->menu_label)
            gtk_entry_set_text(GTK_ENTRY(ctxt->item_name), set->menu_label);
        else if (set->tool > XSetTool::CUSTOM)
            gtk_entry_set_text(GTK_ENTRY(ctxt->item_name),
                               xset_get_builtin_toolitem_label(set->tool));
    }
    else
        gtk_widget_set_sensitive(ctxt->item_name, false);
    // key
    if (rset->menu_style < XSetMenu::SUBMENU || set->tool == XSetTool::BACK_MENU ||
        set->tool == XSetTool::FWD_MENU)
    {
        std::string str2;
        XSet* keyset;
        if (set->shared_key)
            keyset = xset_get(set->shared_key);
        else
            keyset = set;
        str2 = xset_get_keyname(keyset, 0, 0);
        gtk_button_set_label(GTK_BUTTON(ctxt->item_key), str2.c_str());
    }
    else
        gtk_widget_set_sensitive(ctxt->item_key, false);
    // icon
    if (rset->icon || mset->icon)
        gtk_entry_set_text(GTK_ENTRY(ctxt->item_icon), mset->icon ? mset->icon : rset->icon);
    gtk_widget_set_sensitive(ctxt->item_icon,
                             rset->menu_style != XSetMenu::RADIO &&
                                 rset->menu_style != XSetMenu::SEP);
    // toolbar checkbox items have icon
    //( rset->menu_style != XSetMenu::CHECK || rset->tool ) );
    gtk_widget_set_sensitive(ctxt->icon_choose_btn,
                             rset->menu_style != XSetMenu::RADIO &&
                                 rset->menu_style != XSetMenu::SEP);

    if (set->plugin)
    {
        gtk_widget_set_sensitive(ctxt->item_type, false);
        gtk_widget_set_sensitive(ctxt->item_name, false);
        gtk_widget_set_sensitive(ctxt->item_target, false);
        gtk_widget_set_sensitive(ctxt->item_browse, false);
        gtk_widget_set_sensitive(ctxt->cmd_opt_normal, false);
        gtk_widget_set_sensitive(ctxt->cmd_opt_checkbox, false);
        gtk_widget_set_sensitive(ctxt->cmd_opt_confirm, false);
        gtk_widget_set_sensitive(ctxt->cmd_opt_input, false);
        gtk_widget_set_sensitive(ctxt->cmd_user, false);
        gtk_widget_set_sensitive(ctxt->cmd_msg, false);
        gtk_widget_set_sensitive(ctxt->cmd_opt_script, false);
        gtk_widget_set_sensitive(ctxt->cmd_opt_line, false);
    }
    if (set->tool != XSetTool::NOT)
    {
        // Hide Context tab
        gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 1));
        // gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->show_tool ),
        //                              set->tool == XSetB::XSET_B_TRUE );
    }
    // else
    //    gtk_widget_hide( ctxt->show_tool );

    // signals
    // clang-format off
    g_signal_connect(G_OBJECT(ctxt->opt_terminal), "toggled", G_CALLBACK(on_cmd_opt_toggled), ctxt);
    g_signal_connect(G_OBJECT(ctxt->opt_task), "toggled", G_CALLBACK(on_cmd_opt_toggled), ctxt);
    g_signal_connect(G_OBJECT(ctxt->cmd_opt_normal), "toggled", G_CALLBACK(on_cmd_opt_toggled), ctxt);
    g_signal_connect(G_OBJECT(ctxt->cmd_opt_checkbox), "toggled", G_CALLBACK(on_cmd_opt_toggled), ctxt);
    g_signal_connect(G_OBJECT(ctxt->cmd_opt_confirm), "toggled", G_CALLBACK(on_cmd_opt_toggled), ctxt);
    g_signal_connect(G_OBJECT(ctxt->cmd_opt_input), "toggled", G_CALLBACK(on_cmd_opt_toggled), ctxt);

    g_signal_connect(G_OBJECT(ctxt->cmd_opt_line), "toggled", G_CALLBACK(on_script_toggled), ctxt);
    g_signal_connect(G_OBJECT(ctxt->cmd_opt_script), "toggled", G_CALLBACK(on_script_toggled), ctxt);

    g_signal_connect(G_OBJECT(ctxt->item_target), "key-press-event", G_CALLBACK(on_target_keypress), ctxt);
    g_signal_connect(G_OBJECT(ctxt->item_key), "clicked", G_CALLBACK(on_key_button_clicked), ctxt);
    g_signal_connect(G_OBJECT(ctxt->icon_choose_btn), "clicked", G_CALLBACK(on_icon_choose_button_clicked), ctxt);
    g_signal_connect(G_OBJECT(ctxt->item_choose), "clicked", G_CALLBACK(on_browse_button_clicked), ctxt);
    g_signal_connect(G_OBJECT(ctxt->item_browse),"clicked", G_CALLBACK(on_browse_button_clicked), ctxt);
    g_signal_connect(G_OBJECT(ctxt->item_name), "activate", G_CALLBACK(on_entry_activate), ctxt);
    g_signal_connect(G_OBJECT(ctxt->item_icon), "activate", G_CALLBACK(on_entry_activate), ctxt);

    g_signal_connect(G_OBJECT(ctxt->open_browser), "changed", G_CALLBACK(on_open_browser), ctxt);
    g_signal_connect(G_OBJECT(ctxt->item_type), "changed", G_CALLBACK(on_type_changed), ctxt);

    g_signal_connect(ctxt->notebook, "switch-page", G_CALLBACK(on_prop_notebook_switch_page), ctxt);
    // clang-format on

    // run
    enable_context(ctxt);
    if (page &&
        gtk_widget_get_visible(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), page)))
        gtk_notebook_set_current_page(GTK_NOTEBOOK(ctxt->notebook), page);
    else
        gtk_widget_grab_focus(ctxt->set->plugin ? ctxt->item_icon : ctxt->item_name);

    int response;
    while ((response = gtk_dialog_run(GTK_DIALOG(ctxt->dlg))))
    {
        bool exit_loop = false;
        switch (response)
        {
            case GTK_RESPONSE_OK:
                if (mset->context)
                    free(mset->context);
                mset->context = context_build(ctxt);
                replace_item_props(ctxt);
                exit_loop = true;
                break;
            default:
                exit_loop = true;
                break;
        }
        if (exit_loop)
            break;
    }

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(ctxt->dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        xset_set(XSetName::CONTEXT_DLG, XSetSetSet::X, std::to_string(width).c_str());
        xset_set(XSetName::CONTEXT_DLG, XSetSetSet::Y, std::to_string(height).c_str());
    }

    gtk_widget_destroy(ctxt->dlg);
    free(ctxt->temp_cmd_line);
    g_slice_free(ContextData, ctxt);
}
