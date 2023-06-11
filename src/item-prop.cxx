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

#include <optional>
#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <array>
#include <vector>

#include <iostream>
#include <fstream>

#include <glibmm.h>

#include <gtk/gtk.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

#include "mime-type/mime-type.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "xset/xset.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-context.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-plugins.hxx"

#include "item-prop.hxx"
#include "write.hxx"
#include "utils.hxx"

#include "ptk/ptk-app-chooser.hxx"

#include "settings.hxx"

const char* enter_command_use =
    "Enter program or fish command line(s):\n\nUse:\n\t%F\tselected files  or  %f first "
    "selected file\n\t%N\tselected filenames  or  %n first selected filename\n\t%d\tcurrent "
    "directory\n\t%v\tselected device (eg /dev/sda1)\n\t%m\tdevice mount point (eg /media/dvd); "
    " %l device label\n\t%b\tselected bookmark\n\t%t\tselected task directory;  %p task "
    "pid\n\t%a\tmenu item value\n\t$fm_panel, $fm_tab, etc";

namespace item_prop::context
{
    enum class column
    {
        disp,
        sub,
        comp,
        value,
    };

    enum class comparison
    {
        equals,
        nequals,
        contains,
        ncontains,
        begins,
        nbegins,
        ends,
        nends,
        less,
        greater,
        match,
        nmatch,
    };

    enum class item_type
    {
        bookmark,
        app,
        command,
        invalid // Must be last
    };
} // namespace item_prop::context

struct ContextData
{
    ContextData() = default;
    ~ContextData();

    GtkWidget* dlg{nullptr};
    GtkWidget* parent{nullptr};
    GtkWidget* notebook{nullptr};
    xset_context_t context{nullptr};
    xset_t set{nullptr};
    std::string temp_cmd_line{};
    ztd::stat script_stat;
    bool script_stat_valid{false};
    bool reset_command{false};

    // Menu Item Page
    GtkWidget* item_type{nullptr};
    GtkWidget* item_name{nullptr};
    GtkWidget* item_key{nullptr};
    GtkWidget* item_icon{nullptr};
    GtkWidget* target_vbox{nullptr};
    GtkWidget* target_label{nullptr};
    GtkWidget* item_target{nullptr};
    GtkWidget* item_choose{nullptr};
    GtkWidget* item_browse{nullptr};
    GtkWidget* icon_choose_btn{nullptr};

    // Context Page
    GtkWidget* vbox_context{nullptr};
    GtkWidget* view{nullptr};
    GtkButton* btn_remove{nullptr};
    GtkButton* btn_add{nullptr};
    GtkButton* btn_apply{nullptr};
    GtkButton* btn_ok{nullptr};

    GtkWidget* box_sub{nullptr};
    GtkWidget* box_comp{nullptr};
    GtkWidget* box_value{nullptr};
    GtkWidget* box_match{nullptr};
    GtkWidget* box_action{nullptr};
    GtkLabel* current_value{nullptr};
    GtkLabel* test{nullptr};

    GtkWidget* hbox_match{nullptr};
    GtkFrame* frame{nullptr};
    GtkWidget* ignore_context{nullptr};
    GtkWidget* hbox_opener{nullptr};
    GtkWidget* opener{nullptr};

    // Command Page
    GtkWidget* cmd_opt_line{nullptr};
    GtkWidget* cmd_opt_script{nullptr};
    GtkWidget* cmd_edit{nullptr};
    GtkWidget* cmd_edit_root{nullptr};
    GtkWidget* cmd_line_label{nullptr};
    GtkWidget* cmd_scroll_script{nullptr};
    GtkWidget* cmd_script{nullptr};
    GtkWidget* cmd_opt_normal{nullptr};
    GtkWidget* cmd_opt_checkbox{nullptr};
    GtkWidget* cmd_opt_confirm{nullptr};
    GtkWidget* cmd_opt_input{nullptr};
    GtkWidget* cmd_vbox_msg{nullptr};
    GtkWidget* cmd_scroll_msg{nullptr};
    GtkWidget* cmd_msg{nullptr};
    GtkWidget* opt_terminal{nullptr};
    GtkWidget* opt_keep_term{nullptr};
    GtkWidget* cmd_user{nullptr};
    GtkWidget* opt_task{nullptr};
    GtkWidget* opt_task_pop{nullptr};
    GtkWidget* opt_task_err{nullptr};
    GtkWidget* opt_task_out{nullptr};
    GtkWidget* opt_scroll{nullptr};
    GtkWidget* opt_hbox_task{nullptr};
    GtkWidget* open_browser{nullptr};
};

ContextData::~ContextData()
{
    gtk_widget_destroy(this->dlg);
}

// clang-format off
inline constexpr  std::array<const std::string_view, 38> context_subs
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

inline constexpr  std::array<const std::string_view, 38> context_sub_lists
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

const std::map<item_prop::context::comparison, const std::string_view> context_comparisons
{
    {item_prop::context::comparison::equals, "equals"},
    {item_prop::context::comparison::nequals, "does not equal"},
    {item_prop::context::comparison::contains, "contains"},
    {item_prop::context::comparison::ncontains, "does not contain"},
    {item_prop::context::comparison::begins, "begins with"},
    {item_prop::context::comparison::nbegins, "does not begin with"},
    {item_prop::context::comparison::ends, "ends with"},
    {item_prop::context::comparison::nends, "does not end with"},
    {item_prop::context::comparison::less, "is less than"},
    {item_prop::context::comparison::greater, "is greater than"},
    {item_prop::context::comparison::match, "matches"},
    {item_prop::context::comparison::nmatch, "does not match"},
};

inline constexpr std::array<const std::string_view, 3> item_types
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
    {
        return nullptr;
    }
    char* sep = strstr(*s, "%%%%%");
    if (!sep)
    {
        if (*s[0] == '\0')
        {
            return (*s = nullptr);
        }
        ret = ztd::strdup(*s);
        *s = nullptr;
        return ret;
    }
    ret = strndup(*s, sep - *s);
    *s = sep + 5;
    return ret;
}

static bool
get_rule_next(char** s, i32* sub, i32* comp, char** value)
{
    char* vs;
    vs = get_element_next(s);
    if (!vs)
    {
        return false;
    }
    *sub = std::stoi(vs);
    std::free(vs);
    if (*sub < 0 || *sub >= (i32)context_subs.size())
    {
        return false;
    }
    vs = get_element_next(s);
    *comp = std::stoi(vs);
    std::free(vs);
    if (*comp < 0 || *comp >= (i32)context_comparisons.size())
    {
        return false;
    }
    if (!(*value = get_element_next(s)))
    {
        *value = ztd::strdup("");
    }
    return true;
}

item_prop::context::state
xset_context_test(const xset_context_t& context, const std::string_view rules, bool def_disable)
{
    ztd::logger::debug("xset_context_test={}", rules);
    // assumes valid xset_context and rules != nullptr and no global ignore
    i32 i;
    i32 sep_type;
    char* s;
    char* eleval;
    char* sep;
    bool test;

    enum context_test
    {
        any,
        all,
        nany,
        nall
    };

    // get valid action and match
    char* elements = ztd::strdup(rules.data());

    s = get_element_next(&elements);
    if (!s)
    {
        return item_prop::context::state::show;
    }
    const i32 check_action = std::stoi(s);
    std::free(s);
    if (check_action < 0 || check_action > 3)
    {
        return item_prop::context::state::show;
    }
    const item_prop::context::state action = item_prop::context::state(check_action);

    s = get_element_next(&elements);
    if (!s)
    {
        return item_prop::context::state::show;
    }
    const i32 match = std::stoi(s);
    std::free(s);
    if (match < 0 || match > 3)
    {
        return item_prop::context::state::show;
    }

    if (action != item_prop::context::state::hide && action != item_prop::context::state::show &&
        def_disable)
    {
        return item_prop::context::state::disable;
    }

    // parse rules
    i32 sub;
    i32 comp;
    char* value;

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
            {
                sep_type = 1;
            }
            else if ((sep = strstr(eleval, "&&")))
            {
                sep_type = 2;
            }

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

            switch (item_prop::context::comparison(comp))
            {
                case item_prop::context::comparison::equals:
                    test = ztd::same(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::nequals:
                    test = !ztd::same(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::contains:
                    test = ztd::contains(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::ncontains:
                    test = !ztd::contains(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::begins:
                    test = ztd::startswith(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::nbegins:
                    test = !ztd::startswith(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::ends:
                    test = ztd::endswith(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::nends:
                    test = !ztd::endswith(context->var[sub], eleval);
                    break;
                case item_prop::context::comparison::less:
                    test = std::stoi(context->var[sub]) < std::stoi(eleval);
                    break;
                case item_prop::context::comparison::greater:
                    test = std::stoi(context->var[sub]) > std::stoi(eleval);
                    break;
                case item_prop::context::comparison::match:
                case item_prop::context::comparison::nmatch:
                    s = g_utf8_strdown(eleval, -1);
                    if (ztd::compare(eleval, s))
                    {
                        // pattern contains uppercase chars - test case sensitive
                        test = !ztd::fnmatch(eleval, context->var[sub]);
                    }
                    else
                    {
                        // case insensitive
                        const std::string str = ztd::lower(context->var[sub]);
                        test = !ztd::fnmatch(s, str);
                    }
                    std::free(s);
                    if (item_prop::context::comparison(comp) ==
                        item_prop::context::comparison::match)
                    {
                        test = !test;
                    }
                    break;
                default:
                    test = match == context_test::nany || match == context_test::nall; // failsafe
            }

            if (sep)
            {
                if (test)
                {
                    if (sep_type == 1)
                    { // ||
                        break;
                    }
                }
                else
                {
                    if (sep_type == 2)
                    { // &&
                        break;
                    }
                }
                eleval = sep + 2;
                while (eleval[0] == ' ')
                {
                    eleval++;
                }
            }
            else
            {
                eleval[0] = '\0';
            }
        } while (eleval[0] != '\0');
        std::free(value);

        if (test)
        {
            any_match = true;
            no_match = false;
            if (match == context_test::any || match == context_test::nany ||
                match == context_test::nall)
            {
                break;
            }
        }
        else
        {
            all_match = false;
            if (match == context_test::all)
            {
                break;
            }
        }
    }

    if (!is_rules)
    {
        return item_prop::context::state::show;
    }

    bool is_match;
    switch (match)
    {
        case context_test::all:
            is_match = all_match;
            break;
        case context_test::nall:
            is_match = !any_match;
            break;
        case context_test::nany:
            is_match = no_match;
            break;
        case context_test::any:
            is_match = !no_match;
            break;
    }

    switch (action)
    {
        case item_prop::context::state::show:
            return is_match ? item_prop::context::state::show : item_prop::context::state::hide;
        case item_prop::context::state::enable:
            return is_match ? item_prop::context::state::show : item_prop::context::state::disable;
        case item_prop::context::state::disable:
            return is_match ? item_prop::context::state::disable : item_prop::context::state::show;
        case item_prop::context::state::hide:
            break;
    }
    // item_prop::context::state::hide
    if (is_match)
    {
        return item_prop::context::state::hide;
    }
    return def_disable ? item_prop::context::state::disable : item_prop::context::state::show;
}

static std::string
context_build(ContextData* ctxt)
{
    GtkTreeIter it;
    std::string new_context;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        new_context = std::format("{}%%%%%{}",
                                  gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_action)),
                                  gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_match)));
        do
        {
            char* value;
            i32 sub, comp;
            gtk_tree_model_get(model,
                               &it,
                               item_prop::context::column::value,
                               &value,
                               item_prop::context::column::sub,
                               &sub,
                               item_prop::context::column::comp,
                               &comp,
                               -1);
            new_context = std::format("{}%%%%%{}%%%%%{}%%%%%{}", new_context, sub, comp, value);
        } while (gtk_tree_model_iter_next(model, &it));
    }
    return new_context;
}

static void
enable_context(ContextData* ctxt)
{
    const bool is_sel =
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
        const std::string rules = context_build(ctxt);
        std::string text = "Current: Show";
        if (!rules.empty())
        {
            const item_prop::context::state action = xset_context_test(ctxt->context, rules, false);
            if (action == item_prop::context::state::hide)
            {
                text = "Current: Hide";
            }
            else if (action == item_prop::context::state::disable)
            {
                text = "Current: Disable";
            }
            else if (action == item_prop::context::state::show &&
                     gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_action)) ==
                         magic_enum::enum_integer(item_prop::context::state::disable))
            {
                text = "Current: Enable";
            }
        }
        gtk_label_set_text(ctxt->test, text.data());
    }
}

static void
on_context_action_changed(GtkComboBox* box, ContextData* ctxt)
{
    (void)box;
    enable_context(ctxt);
}

static const std::string
context_display(i32 sub, i32 comp, const char* value)
{
    if (value[0] == '\0' || value[0] == ' ' || ztd::endswith(value, " "))
    {
        return std::format("{} {} \"{}\"",
                           context_subs.at(sub),
                           context_comparisons.at(item_prop::context::comparison(comp)),
                           value);
    }
    else
    {
        return std::format("{} {} {}",
                           context_subs.at(sub),
                           context_comparisons.at(item_prop::context::comparison(comp)),
                           value);
    }
}

static void
on_context_button_press(GtkWidget* widget, ContextData* ctxt)
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;

    if (widget == GTK_WIDGET(ctxt->btn_add) || widget == GTK_WIDGET(ctxt->btn_apply))
    {
        const i32 sub = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_sub));
        const i32 comp = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_comp));
        if (sub < 0 || comp < 0)
        {
            return;
        }
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
        if (widget == GTK_WIDGET(ctxt->btn_add))
        {
            gtk_list_store_append(GTK_LIST_STORE(model), &it);
        }
        else
        {
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view));
            if (!gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
            {
                return;
            }
        }
        char* value = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ctxt->box_value));
        const std::string disp = context_display(sub, comp, value);
        gtk_list_store_set(GTK_LIST_STORE(model),
                           &it,
                           item_prop::context::column::disp,
                           disp.c_str(),
                           item_prop::context::column::sub,
                           sub,
                           item_prop::context::column::comp,
                           comp,
                           item_prop::context::column::value,
                           value,
                           -1);
        std::free(value);
        gtk_widget_set_sensitive(GTK_WIDGET(ctxt->btn_ok), true);
        if (widget == GTK_WIDGET(ctxt->btn_add))
        {
            gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view)),
                                           &it);
        }
        enable_context(ctxt);
        return;
    }

    // remove
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctxt->view));
    if (gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
    {
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);
    }

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
    {
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);
    }

    const i32 sub = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->box_sub));
    if (sub < 0)
    {
        return;
    }
    char* elements = ztd::strdup(context_sub_lists[sub].data());
    char* def_comp = get_element_next(&elements);
    if (def_comp)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_comp), std::stoi(def_comp));
        std::free(def_comp);
    }
    while ((value = get_element_next(&elements)))
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_value), value);
        std::free(value);
    }
    gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(ctxt->box_value))), "");
    if (ctxt->context && ctxt->context->valid)
    {
        gtk_label_set_text(ctxt->current_value, ctxt->context->var[sub].data());
    }

    std::free(elements);
}

static void
on_context_row_activated(GtkTreeView* view, GtkTreePath* tree_path, GtkTreeViewColumn* col,
                         ContextData* ctxt)
{
    (void)view;
    (void)col;
    GtkTreeIter it;
    char* value;
    i32 sub, comp;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ctxt->view));
    if (!gtk_tree_model_get_iter(model, &it, tree_path))
    {
        return;
    }
    gtk_tree_model_get(model,
                       &it,
                       item_prop::context::column::value,
                       &value,
                       item_prop::context::column::sub,
                       &sub,
                       item_prop::context::column::comp,
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
    if (event->type == GdkEventType::GDK_2BUTTON_PRESS && event->button == 1)
    {
        gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(ctxt->box_value))),
                           gtk_label_get_text(ctxt->current_value));
        gtk_widget_grab_focus(ctxt->box_value);
        return true;
    }
    return false;
}

static void
on_context_entry_insert(GtkEntryBuffer* buf, u32 position, char* chars, u32 n_chars,
                        void* user_data)
{ // remove linefeeds from pasted text
    (void)position;
    (void)chars;
    (void)n_chars;
    (void)user_data;

    if (!ztd::contains(gtk_entry_buffer_get_text(buf), "\n"))
    {
        return;
    }

    const std::string new_text = ztd::replace(gtk_entry_buffer_get_text(buf), "\n", "");
    gtk_entry_buffer_set_text(buf, new_text.data(), -1);
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
        {
            on_context_button_press(GTK_WIDGET(ctxt->btn_apply), ctxt);
        }
        else
        {
            on_context_button_press(GTK_WIDGET(ctxt->btn_add), ctxt);
        }
        return true;
    }
    return false;
}

static void
enable_options(ContextData* ctxt)
{
    gtk_widget_set_sensitive(ctxt->opt_keep_term,
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_terminal)));
    const bool as_task = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task));
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
            ctxt->set->menu_style != xset::menu::sep &&
            ctxt->set->menu_style != xset::menu::submenu);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_confirm)))
    {
        // add default msg
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_msg));
        if (gtk_text_buffer_get_char_count(buf) == 0)
        {
            gtk_text_buffer_set_text(buf, "Are you sure?", -1);
        }
    }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_input)))
    {
        // remove default msg
        GtkTextIter iter, siter;
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_msg));
        gtk_text_buffer_get_start_iter(buf, &siter);
        gtk_text_buffer_get_end_iter(buf, &iter);
        char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
        if (text && ztd::same(text, "Are you sure?"))
        {
            gtk_text_buffer_set_text(buf, "", -1);
        }
        std::free(text);
    }
}

static bool
is_command_script_newer(ContextData* ctxt)
{
    if (!ctxt->script_stat_valid)
    {
        return false;
    }
    const char* script = xset_custom_get_script(ctxt->set, false);
    if (!script)
    {
        return false;
    }

    const auto script_stat = ztd::stat(script);
    if (!script_stat.is_valid())
    {
        return false;
    }

    if (!ctxt->script_stat.is_valid())
    {
        return true;
    }

    if (script_stat.mtime() != ctxt->script_stat.mtime() ||
        script_stat.size() != ctxt->script_stat.size())
    {
        return true;
    }

    return false;
}

void
command_script_stat(ContextData* ctxt)
{
    const char* script = xset_custom_get_script(ctxt->set, false);
    if (!script)
    {
        ctxt->script_stat_valid = false;
        return;
    }

    const auto script_stat = ztd::stat(script);
    if (script_stat.is_valid())
    {
        ctxt->script_stat_valid = true;
    }
    else
    {
        ctxt->script_stat_valid = false;
    }
}

void
load_text_view(GtkTextView* view, const std::string_view line)
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer(view);
    if (line.empty())
    {
        gtk_text_buffer_set_text(buf, "", -1);
        return;
    }
    std::string text = line.data();
    text = ztd::replace(text, "\\n", "\n");
    text = ztd::replace(text, "\\t", "\t");
    gtk_text_buffer_set_text(buf, text.data(), -1);
}

const std::string
get_text_view(GtkTextView* view)
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer(view);
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (!(text && text[0]))
    {
        std::free(text);
        return nullptr;
    }
    std::string text_view = text;
    text_view = ztd::replace(text_view, "\\n", "\n");
    text_view = ztd::replace(text_view, "\\t", "\t");
    return text_view;
}

static void
load_command_script(ContextData* ctxt, xset_t set)
{
    bool modified = false;
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
    char* script = xset_custom_get_script(set, !set->plugin);
    gtk_text_buffer_set_text(buf, "", -1);
    if (script)
    {
        std::ifstream file(script);
        if (!file)
        {
            ztd::logger::error("Failed to open the file: {}", script);
            return;
        }

        std::string line;
        while (std::getline(file, line))
        {
            // read file one line at a time to prevent splitting UTF-8 characters
            if (!g_utf8_validate(line.data(), -1, nullptr))
            {
                file.close();
                gtk_text_buffer_set_text(buf, "", -1);
                modified = true;
                ztd::logger::warn("file '{}' contents are not valid UTF-8", script);
                break;
            }
            gtk_text_buffer_insert_at_cursor(buf, line.data(), -1);
        }
        file.close();
    }
    const bool have_access = script && have_rw_access(script);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ctxt->cmd_script), !set->plugin && have_access);
    gtk_text_buffer_set_modified(buf, modified);
    command_script_stat(ctxt);
    std::free(script);
    if (have_access && geteuid() != 0)
    {
        gtk_widget_hide(ctxt->cmd_edit_root);
    }
    else
    {
        gtk_widget_show(ctxt->cmd_edit_root);
    }
}

static void
save_command_script(ContextData* ctxt, bool query)
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
    if (!gtk_text_buffer_get_modified(buf))
    {
        return;
    }

    if (query)
    {
        const i32 response = xset_msg_dialog(ctxt->dlg,
                                             GtkMessageType::GTK_MESSAGE_QUESTION,
                                             "Save Modified Script?",
                                             GtkButtonsType::GTK_BUTTONS_YES_NO,
                                             "Save your changes to the command script?");

        if (response == GtkResponseType::GTK_RESPONSE_NO)
        {
            return;
        }
    }

    if (is_command_script_newer(ctxt))
    {
        const i32 response = xset_msg_dialog(
            ctxt->dlg,
            GtkMessageType::GTK_MESSAGE_QUESTION,
            "Overwrite Script?",
            GtkButtonsType::GTK_BUTTONS_YES_NO,
            "The command script on disk has changed.\n\nDo you want to overwrite it?");

        if (response == GtkResponseType::GTK_RESPONSE_NO)
        {
            return;
        }
    }

    char* script = xset_custom_get_script(ctxt->set, false);
    if (!script)
    {
        return;
    }

    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);

    write_file(script, text);

    std::free(text);
}

static void
on_script_toggled(GtkWidget* item, ContextData* ctxt)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
    {
        return;
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_line)))
    {
        // set to command line
        save_command_script(ctxt, true);
        gtk_widget_show(ctxt->cmd_line_label);
        gtk_widget_show(ctxt->cmd_edit_root);
        load_text_view(GTK_TEXT_VIEW(ctxt->cmd_script), ctxt->temp_cmd_line.c_str());
    }
    else
    {
        // set to script
        gtk_widget_hide(ctxt->cmd_line_label);
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
    {
        gtk_widget_grab_focus(ctxt->cmd_msg);
    }
    else if (item == ctxt->opt_terminal && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item)))
    {
        // checking run in terminal unchecks run as task
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->opt_task), false);
    }
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
    std::string path;
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_line)))
    {
        // set to command line - get path of first argument
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->cmd_script));
        GtkTextIter iter, siter;
        gtk_text_buffer_get_start_iter(buf, &siter);
        gtk_text_buffer_get_end_iter(buf, &iter);
        char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
        if (text)
        {
            path = ztd::strip(text);
            if (!ztd::startswith(path, "/"))
            {
                path = Glib::find_program_in_path(path);
            }
        }
        std::free(text);
        if (path.empty() || !mime_type_is_text_file(path, ""))
        {
            xset_msg_dialog(GTK_WIDGET(ctxt->dlg),
                            GtkMessageType::GTK_MESSAGE_ERROR,
                            "Error",
                            GtkButtonsType::GTK_BUTTONS_OK,
                            "The command line does not begin with a text file (script) to be "
                            "opened, or the script was not found in your $PATH.");
            return;
        }
    }
    else
    {
        // set to script
        save_command_script(ctxt, false);
        path = xset_custom_get_script(ctxt->set, !ctxt->set->plugin);
    }

    if (mime_type_is_text_file(path, ""))
    {
        xset_edit(ctxt->dlg, path.data(), btn == ctxt->cmd_edit_root, btn != ctxt->cmd_edit_root);
    }
}

static void
on_open_browser(GtkComboBox* box, ContextData* ctxt)
{
    std::filesystem::path folder;
    const i32 job = gtk_combo_box_get_active(GTK_COMBO_BOX(box));
    gtk_combo_box_set_active(GTK_COMBO_BOX(box), -1);
    if (job == 0)
    {
        // Command Dir
        if (ctxt->set->plugin)
        {
            folder = ctxt->set->plugin->path / "files";
            if (!std::filesystem::exists(folder))
            {
                folder = ctxt->set->plugin->path / ctxt->set->plugin->name;
            }
        }
        else
        {
            folder = vfs::user_dirs->program_config_dir() / "scripts" / ctxt->set->name;
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
            xset_t mset = xset_get_plugin_mirror(ctxt->set);
            folder = vfs::user_dirs->program_config_dir() / "plugin-data" / mset->name;
        }
        else
        {
            folder = vfs::user_dirs->program_config_dir() / "plugin-data" / ctxt->set->name;
        }
        if (!std::filesystem::exists(folder))
        {
            std::filesystem::create_directories(folder);
            std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
        }
    }
    else if (job == 2)
    {
        // Plugin Dir
        if (ctxt->set->plugin && !ctxt->set->plugin->path.empty())
        {
            folder = ctxt->set->plugin->path;
        }
    }
    else
    {
        return;
    }

    if (std::filesystem::is_directory(folder))
    {
        open_in_prog(folder);
    }
}

static void
on_key_button_clicked(GtkWidget* widget, ContextData* ctxt)
{
    (void)widget;
    xset_set_key(ctxt->dlg, ctxt->set);

    xset_t keyset;
    if (ctxt->set->shared_key)
    {
        keyset = xset_get(ctxt->set->shared_key.value());
    }
    else
    {
        keyset = ctxt->set;
    }
    const std::string str = xset_get_keyname(keyset, 0, 0);
    gtk_button_set_label(GTK_BUTTON(ctxt->item_key), str.data());
}

static void
on_type_changed(GtkComboBox* box, ContextData* ctxt)
{
    xset_t rset = ctxt->set;
    xset_t mset = xset_get_plugin_mirror(rset);
    const item_prop::context::item_type job =
        item_prop::context::item_type(gtk_combo_box_get_active(GTK_COMBO_BOX(box)));
    switch (job)
    {
        case item_prop::context::item_type::bookmark:
        case item_prop::context::item_type::app:
            // Bookmark or App
            gtk_widget_show(ctxt->target_vbox);
            gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 2));
            gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 3));

            if (job == item_prop::context::item_type::bookmark)
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
        case item_prop::context::item_type::command:
            // Command
            gtk_widget_hide(ctxt->target_vbox);
            gtk_widget_show(ctxt->hbox_opener);
            gtk_widget_show(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 2));
            gtk_widget_show(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 3));
            break;
        case item_prop::context::item_type::invalid:
            break;
    }

    // load command data
    if (rset->x && xset::cmd(std::stoi(rset->x.value())) == xset::cmd::script)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_script), true);
        gtk_widget_hide(ctxt->cmd_line_label);
        load_command_script(ctxt, rset);
    }
    else
    {
        load_text_view(GTK_TEXT_VIEW(ctxt->cmd_script), rset->line.value());
    }
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
    gtk_entry_set_text(GTK_ENTRY(ctxt->cmd_user), rset->y.value_or("").c_str());
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
    if (rset->menu_style == xset::menu::check)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_checkbox), true);
    }
    else if (rset->menu_style == xset::menu::confirm)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_confirm), true);
    }
    else if (rset->menu_style == xset::menu::string)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_input), true);
    }
    else
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_normal), true);
    }
    load_text_view(GTK_TEXT_VIEW(ctxt->cmd_msg), rset->desc.value());
    enable_options(ctxt);
    if (geteuid() == 0)
    {
        // running as root
        gtk_widget_hide(ctxt->cmd_edit);
    }

    // TODO switch?
    if (job < item_prop::context::item_type::command)
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

    if (job == item_prop::context::item_type::command || job == item_prop::context::item_type::app)
    {
        // Opener
        if (mset->opener > 2 || mset->opener < 0)
        {
            // forwards compat
            gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->opener), -1);
        }
        else
        {
            gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->opener), mset->opener);
        }
    }
}

static void
on_browse_button_clicked(GtkWidget* widget, ContextData* ctxt)
{
    const item_prop::context::item_type job =
        item_prop::context::item_type(gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->item_type)));
    if (job == item_prop::context::item_type::bookmark)
    {
        // Bookmark Browse
        const auto add_path =
            xset_file_dialog(ctxt->dlg,
                             GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                             "Choose Directory",
                             ctxt->context->var[item_prop::context::item::dir],
                             std::nullopt);
        if (add_path)
        {
            const auto old_path = multi_input_get_text(ctxt->item_target);
            const std::string new_path = std::format("{}{}{}",
                                                     old_path ? old_path.value() : "",
                                                     old_path ? "; " : "",
                                                     add_path.value().string());
            GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->item_target));
            gtk_text_buffer_set_text(buf, new_path.data(), -1);
        }
    }
    else
    {
        // Application
        if (widget == ctxt->item_choose)
        {
            // Choose
            vfs::mime_type mime_type = vfs_mime_type_get_from_type(
                !ctxt->context->var[item_prop::context::item::mime].empty()
                    ? ctxt->context->var[item_prop::context::item::mime]
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
            std::free(app);
        }
        else
        {
            // Browse
            const auto exec_path =
                xset_file_dialog(ctxt->dlg,
                                 GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN,
                                 "Choose Executable",
                                 "/usr/bin",
                                 std::nullopt);
            if (exec_path)
            {
                GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctxt->item_target));
                gtk_text_buffer_set_text(buf, exec_path.value().c_str(), -1);
            }
        }
    }
}

static void
replace_item_props(ContextData* ctxt)
{
    xset::cmd x;
    xset_t rset = ctxt->set;
    xset_t mset = xset_get_plugin_mirror(rset);

    if (!rset->lock && rset->menu_style != xset::menu::submenu &&
        rset->menu_style != xset::menu::sep && rset->tool <= xset::tool::custom)
    {
        // custom bookmark, app, or command
        bool is_app = false;
        const item_prop::context::item_type item_type =
            item_prop::context::item_type(gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->item_type)));

        switch (item_type)
        {
            case item_prop::context::item_type::bookmark:
                x = xset::cmd::bookmark;
                is_app = true;
                break;
            case item_prop::context::item_type::app:
                x = xset::cmd::app;
                is_app = true;
                break;
            case item_prop::context::item_type::command:
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_line)))
                {
                    // line
                    x = xset::cmd::line;
                }
                else
                {
                    // script
                    x = xset::cmd::script;
                    save_command_script(ctxt, false);
                }
                break;
            case item_prop::context::item_type::invalid:
                x = xset::cmd::invalid;
                break;
        }

        if (x != xset::cmd::invalid)
        {
            if (x == xset::cmd::line)
            {
                rset->x = std::nullopt;
            }
            else
            {
                rset->x = std::to_string(magic_enum::enum_integer(x));
            }
        }
        if (!rset->plugin)
        {
            // target
            const auto str = multi_input_get_text(ctxt->item_target);
            if (str)
            {
                rset->z = ztd::strip(str.value());
            }
            else
            {
                rset->z = std::nullopt;
            }
            // run as user
            const char* text = gtk_entry_get_text(GTK_ENTRY(ctxt->cmd_user));
            if (text)
            {
                rset->y = text;
            }
            // menu style
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_checkbox)))
            {
                rset->menu_style = xset::menu::check;
            }
            else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_confirm)))
            {
                rset->menu_style = xset::menu::confirm;
            }
            else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->cmd_opt_input)))
            {
                rset->menu_style = xset::menu::string;
            }
            else
            {
                rset->menu_style = xset::menu::normal;
            }
            // style msg
            rset->desc = get_text_view(GTK_TEXT_VIEW(ctxt->cmd_msg));
        }
        // command line
        if (x == xset::cmd::line)
        {
            rset->line = get_text_view(GTK_TEXT_VIEW(ctxt->cmd_script));
            if (rset->line && rset->line.value().size() > 2000)
            {
                xset_msg_dialog(ctxt->dlg,
                                GtkMessageType::GTK_MESSAGE_WARNING,
                                "Command Line Too Long",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                "Your command line is greater than 2000 characters and may be "
                                "truncated when saved.  Consider using a command script instead "
                                "by selecting Script on the Command tab.");
            }
        }
        else
        {
            if (!ctxt->temp_cmd_line.empty())
            {
                rset->line = ctxt->temp_cmd_line;
            }
        }

        // run options
        mset->in_terminal =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_terminal)) && !is_app;
        mset->keep_terminal =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_keep_term)) && !is_app;
        mset->task = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task)) && !is_app;
        mset->task_pop =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_pop)) && !is_app;
        mset->task_err =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_err)) && !is_app;
        mset->task_out =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_task_out)) && !is_app;
        mset->scroll_lock =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->opt_scroll)) || is_app;

        // Opener
        if ((item_type == item_prop::context::item_type::command ||
             item_type == item_prop::context::item_type::app))
        {
            if (gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->opener)) > -1)
            {
                mset->opener = gtk_combo_box_get_active(GTK_COMBO_BOX(ctxt->opener));
            }
            // otherwise do not change for forward compat
        }
        else
        {
            // reset if not applicable
            mset->opener = 0;
        }
    }
    if (rset->menu_style != xset::menu::sep && !rset->plugin)
    {
        // name
        if (rset->lock &&
            !ztd::same(rset->menu_label.value(), gtk_entry_get_text(GTK_ENTRY(ctxt->item_name))))
        {
            // built-in label has been changed from default, save it
            rset->in_terminal = true;
        }

        if (rset->tool > xset::tool::custom &&
            ztd::same(gtk_entry_get_text(GTK_ENTRY(ctxt->item_name)),
                      xset_get_builtin_toolitem_label(rset->tool)))
        {
            // do not save default label of builtin toolitems
            rset->menu_label = std::nullopt;
        }
        else
        {
            rset->menu_label = gtk_entry_get_text(GTK_ENTRY(ctxt->item_name));
        }
    }
    // icon
    if (rset->menu_style != xset::menu::radio && rset->menu_style != xset::menu::sep)
    // checkbox items in 1.0.1 allow icon due to bookmark list showing
    // toolbar checkbox items have icon
    //( rset->menu_style != xset::menu::CHECK || rset->tool ) )
    {
        const std::string old_icon = mset->icon.value();
        const char* icon_name = gtk_entry_get_text(GTK_ENTRY(ctxt->item_icon));
        if (icon_name && icon_name[0])
        {
            mset->icon = icon_name;
        }
        else
        {
            mset->icon = std::nullopt;
        }

        if (rset->lock && !ztd::same(old_icon, mset->icon.value()))
        {
            // built-in icon has been changed from default, save it
            rset->keep_terminal = true;
        }
    }

    // Ignore Context
    xset_set_b(xset::name::context_dlg,
               gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctxt->ignore_context)));
}

static void
on_script_popup(GtkTextView* input, GtkMenu* menu, void* user_data)
{
    (void)input;
    (void)user_data;
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_t set = xset_get(xset::name::separator);
    set->menu_style = xset::menu::sep;
    set->browser = nullptr;
    xset_add_menuitem(nullptr, GTK_WIDGET(menu), accel_group, set);

    gtk_widget_show_all(GTK_WIDGET(menu));
}

static bool
delayed_focus(GtkWidget* widget)
{
    if (GTK_IS_WIDGET(widget))
    {
        gtk_widget_grab_focus(widget);
    }
    return false;
}

static void
on_prop_notebook_switch_page(GtkNotebook* notebook, GtkWidget* page, u32 page_num,
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
xset_item_prop_dlg(const xset_context_t& context, xset_t set, i32 page)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;

    if (!context || !set)
    {
        return;
    }
    const auto ctxt = new ContextData;
    ctxt->context = context;
    ctxt->set = set;
    if (set->browser)
    {
        ctxt->parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));
    }

    // Dialog
    ctxt->dlg = gtk_dialog_new_with_buttons(
        set->tool != xset::tool::NOT ? "Toolbar Item Properties" : "Menu Item Properties",
        GTK_WINDOW(ctxt->parent),
        GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                       GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
        nullptr,
        nullptr);
    xset_set_window_icon(GTK_WINDOW(ctxt->dlg));
    gtk_window_set_role(GTK_WINDOW(ctxt->dlg), "context_dialog");

    i32 width = xset_get_int(xset::name::context_dlg, xset::var::x);
    i32 height = xset_get_int(xset::name::context_dlg, xset::var::y);
    if (width && height)
    {
        gtk_window_set_default_size(GTK_WINDOW(ctxt->dlg), width, height);
    }
    else
    {
        gtk_window_set_default_size(GTK_WINDOW(ctxt->dlg), 800, 600);
    }

    gtk_dialog_add_button(GTK_DIALOG(ctxt->dlg), "Cancel", GtkResponseType::GTK_RESPONSE_CANCEL);
    ctxt->btn_ok = GTK_BUTTON(
        gtk_dialog_add_button(GTK_DIALOG(ctxt->dlg), "OK", GtkResponseType::GTK_RESPONSE_OK));

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
    GtkWidget* vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 0);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_notebook_append_page(
        GTK_NOTEBOOK(ctxt->notebook),
        vbox,
        gtk_label_new_with_mnemonic(set->tool != xset::tool::NOT ? "_Toolbar Item" : "_Menu Item"));

    GtkGrid* grid = GTK_GRID(gtk_grid_new());
    gtk_container_set_border_width(GTK_CONTAINER(grid), 0);
    gtk_grid_set_row_spacing(grid, 6);
    gtk_grid_set_column_spacing(grid, 8);
    i32 row = 0;

    GtkWidget* label = gtk_label_new_with_mnemonic("Type:");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    ctxt->item_type = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_type), false);
    gtk_grid_attach(grid, ctxt->item_type, 1, row, 1, 1);

    label = gtk_label_new_with_mnemonic("Name:");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    row++;
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    ctxt->item_name = gtk_entry_new();
    gtk_grid_attach(grid, ctxt->item_name, 1, row, 1, 1);

    label = gtk_label_new_with_mnemonic("Key:");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    row++;
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    ctxt->item_key = gtk_button_new_with_label(" ");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_key), false);
    gtk_grid_attach(grid, ctxt->item_key, 1, row, 1, 1);

    label = gtk_label_new_with_mnemonic("Icon:");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    row++;
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    GtkWidget* hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
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
    ctxt->target_vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0);

    ctxt->target_label = gtk_label_new(nullptr);
    gtk_widget_set_halign(GTK_WIDGET(ctxt->target_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ctxt->target_label), GtkAlign::GTK_ALIGN_CENTER);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
                                        GtkShadowType::GTK_SHADOW_ETCHED_IN);
    ctxt->item_target = GTK_WIDGET(multi_input_new(GTK_SCROLLED_WINDOW(scroll), nullptr));
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->item_target), -1, 100);
    gtk_widget_set_size_request(GTK_WIDGET(scroll), -1, 100);

    gtk_box_pack_start(GTK_BOX(ctxt->target_vbox), GTK_WIDGET(ctxt->target_label), false, true, 0);
    gtk_box_pack_start(GTK_BOX(ctxt->target_vbox), GTK_WIDGET(scroll), false, true, 0);

    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(gtk_label_new(nullptr)), true, true, 0);
    ctxt->item_choose = gtk_button_new_with_mnemonic("C_hoose");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_choose), false);

    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->item_choose), false, true, 12);

    ctxt->item_browse = gtk_button_new_with_mnemonic("_Browse");
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->item_choose), false);

    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->item_browse), false, true, 0);

    gtk_box_pack_start(GTK_BOX(ctxt->target_vbox), GTK_WIDGET(hbox), false, true, 0);

    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->target_vbox), false, true, 4);

    vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 0);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctxt->notebook),
                             vbox,
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
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll),
                                        GtkShadowType::GTK_SHADOW_ETCHED_IN);
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
    gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        item_prop::context::column::disp,
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
    for (const std::string_view context_sub : context_subs)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_sub), context_sub.data());
    }
    g_signal_connect(G_OBJECT(ctxt->box_sub), "changed", G_CALLBACK(on_context_sub_changed), ctxt);

    ctxt->box_comp = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->box_comp), false);
    for (const auto context_comparison : context_comparisons)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->box_comp),
                                       context_comparison.second.data());
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
    gtk_label_set_ellipsize(ctxt->current_value, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(ctxt->current_value, true);
    gtk_widget_set_halign(GTK_WIDGET(ctxt->current_value), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ctxt->current_value), GtkAlign::GTK_ALIGN_START);
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

    ctxt->vbox_context = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0);
    ctxt->hbox_match = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_match), GTK_WIDGET(ctxt->box_action), false, true, 0);
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_match),
                       GTK_WIDGET(gtk_label_new("item if context")),
                       false,
                       true,
                       4);
    gtk_box_pack_start(GTK_BOX(ctxt->hbox_match), GTK_WIDGET(ctxt->box_match), false, true, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(ctxt->hbox_match), false, true, 4);

    //    GtkLabel* label = gtk_label_new( "Rules:" );
    //    gtk_widget_set_halign(label, GtkAlign::GTK_ALIGN_START);
    //    gtk_widget_set_valign(label, GtkAlign::GTK_ALIGN_END);
    //    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( ctxt->dlg )->vbox ),
    //                        GTK_WIDGET( label ), false, true, 8 );
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(scroll), true, true, 4);

    GtkWidget* hbox_btns = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->btn_remove), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns),
                       GTK_WIDGET(gtk_separator_new(GtkOrientation::GTK_ORIENTATION_VERTICAL)),
                       false,
                       true,
                       4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->btn_add), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->btn_apply), false, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_btns), GTK_WIDGET(ctxt->test), true, true, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(hbox_btns), false, true, 4);

    ctxt->frame = GTK_FRAME(gtk_frame_new("Edit Rule"));
    GtkWidget* vbox_frame = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(ctxt->frame), vbox_frame);
    GtkWidget* hbox_frame = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox_frame), GTK_WIDGET(ctxt->box_sub), false, true, 8);
    gtk_box_pack_start(GTK_BOX(hbox_frame), GTK_WIDGET(ctxt->box_comp), false, true, 4);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox_frame), false, true, 4);
    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->box_value), true, true, 8);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), true, true, 4);
    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(gtk_label_new("Value:")), false, true, 8);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->current_value), true, true, 2);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), true, true, 4);
    gtk_box_pack_start(GTK_BOX(ctxt->vbox_context), GTK_WIDGET(ctxt->frame), false, true, 16);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->vbox_context), true, true, 0);

    // Opener
    ctxt->hbox_opener = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
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
    if (xset_get_b(xset::name::context_dlg))
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctxt->ignore_context), true);

        gtk_widget_set_sensitive(ctxt->vbox_context, false);
    }
    g_signal_connect(G_OBJECT(ctxt->ignore_context),
                     "toggled",
                     G_CALLBACK(on_ignore_context_toggled),
                     ctxt);

    // plugin?
    xset_t mset; // mirror set or set
    xset_t rset; // real set
    if (set->plugin)
    {
        // set is plugin
        mset = xset_get_plugin_mirror(set);
        rset = set;
    }
    else if (!set->lock && set->desc && ztd::same(set->desc.value(), "@plugin@mirror@") &&
             set->shared_key)
    {
        // set is plugin mirror
        mset = set;
        rset = xset_get(set->shared_key.value());
        rset->browser = set->browser;
    }
    else
    {
        mset = set;
        rset = set;
    }
    ctxt->set = rset;

    // set match / action
    char* elements = ztd::strdup(mset->context.value());
    char* action = get_element_next(&elements);
    char* match = get_element_next(&elements);
    if (match && action)
    {
        i32 i = std::stoi(match);
        if (i < 0 || i > 3)
        {
            i = 0;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_match), i);
        i = std::stoi(action);
        if (i < 0 || i > 3)
        {
            i = 0;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_action), i);
        std::free(match);
        std::free(action);
    }
    else
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_match), 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_action), 0);
        if (match)
        {
            std::free(match);
        }
        if (action)
        {
            std::free(action);
        }
    }
    // set rules
    i32 sub, comp;
    char* value;
    GtkTreeIter it;
    while (get_rule_next(&elements, &sub, &comp, &value))
    {
        const std::string disp = context_display(sub, comp, value);
        gtk_list_store_append(GTK_LIST_STORE(list), &it);
        gtk_list_store_set(GTK_LIST_STORE(list),
                           &it,
                           item_prop::context::column::disp,
                           disp.c_str(),
                           item_prop::context::column::sub,
                           sub,
                           item_prop::context::column::comp,
                           comp,
                           item_prop::context::column::value,
                           value,
                           -1);
        if (value)
        {
            std::free(value);
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->box_sub), 0);

    // Command Page  =====================================================
    vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 0);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctxt->notebook),
                             vbox,
                             gtk_label_new_with_mnemonic("Comm_and"));

    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 8);
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
    gtk_widget_set_halign(GTK_WIDGET(ctxt->cmd_line_label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(ctxt->cmd_line_label), GtkAlign::GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->cmd_line_label), false, true, 8);

    // Script
    ctxt->cmd_scroll_script = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_script),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_script),
                                        GtkShadowType::GTK_SHADOW_ETCHED_IN);
    ctxt->cmd_script = GTK_WIDGET(gtk_text_view_new());
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_script), -1, 50);
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_scroll_script), -1, 50);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctxt->cmd_script), GtkWrapMode::GTK_WRAP_WORD_CHAR);
    g_signal_connect_after(G_OBJECT(ctxt->cmd_script),
                           "populate-popup",
                           G_CALLBACK(on_script_popup),
                           nullptr);
    gtk_container_add(GTK_CONTAINER(ctxt->cmd_scroll_script), GTK_WIDGET(ctxt->cmd_script));
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(ctxt->cmd_scroll_script), true, true, 4);

    // Option Page  =====================================================
    vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(vbox), GtkAlign::GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox), true);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 0);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctxt->notebook),
                             vbox,
                             gtk_label_new_with_mnemonic("Optio_ns"));

    GtkWidget* frame = gtk_frame_new("Run Options");
    vbox_frame = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(GTK_WIDGET(vbox_frame), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(vbox_frame), GtkAlign::GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox_frame), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox_frame), true);
    gtk_widget_set_margin_top(vbox_frame, 8);
    gtk_widget_set_margin_bottom(vbox_frame, 0);
    gtk_widget_set_margin_start(vbox_frame, 8);
    gtk_widget_set_margin_end(vbox_frame, 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox_frame);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), false, true, 8);

    ctxt->opt_task = gtk_check_button_new_with_mnemonic("Run As Task");
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(ctxt->opt_task), false, true, 0);
    ctxt->opt_hbox_task = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 8);
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

    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 8);
    ctxt->opt_terminal = gtk_check_button_new_with_mnemonic("Run In Terminal");
    ctxt->opt_keep_term = gtk_check_button_new_with_mnemonic("Keep Terminal Open");
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->opt_terminal), false, true, 0);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->opt_keep_term), false, true, 6);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), false, true, 0);

    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    label = gtk_label_new("Run As User:");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), false, true, 2);
    ctxt->cmd_user = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->cmd_user), false, true, 8);
    label = gtk_label_new("( leave blank for current user )");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), false, true, 8);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(hbox), false, true, 4);

    frame = gtk_frame_new("Style");
    vbox_frame = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_halign(GTK_WIDGET(vbox_frame), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(vbox_frame), GtkAlign::GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(GTK_WIDGET(vbox_frame), true);
    gtk_widget_set_vexpand(GTK_WIDGET(vbox_frame), true);
    gtk_widget_set_margin_top(vbox_frame, 8);
    gtk_widget_set_margin_bottom(vbox_frame, 0);
    gtk_widget_set_margin_start(vbox_frame, 8);
    gtk_widget_set_margin_end(vbox_frame, 8);
    gtk_container_add(GTK_CONTAINER(frame), vbox_frame);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), true, true, 8);

    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 8);
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
    ctxt->cmd_vbox_msg = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 4);
    label = gtk_label_new("Confirmation/Input Message:");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(ctxt->cmd_vbox_msg), GTK_WIDGET(label), false, true, 8);
    ctxt->cmd_scroll_msg = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_msg),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ctxt->cmd_scroll_msg),
                                        GtkShadowType::GTK_SHADOW_ETCHED_IN);
    ctxt->cmd_msg = GTK_WIDGET(gtk_text_view_new());
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_msg), -1, 50);
    gtk_widget_set_size_request(GTK_WIDGET(ctxt->cmd_scroll_msg), -1, 50);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ctxt->cmd_msg), GtkWrapMode::GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(ctxt->cmd_scroll_msg), GTK_WIDGET(ctxt->cmd_msg));
    gtk_box_pack_start(GTK_BOX(ctxt->cmd_vbox_msg),
                       GTK_WIDGET(ctxt->cmd_scroll_msg),
                       true,
                       true,
                       4);
    gtk_box_pack_start(GTK_BOX(vbox_frame), GTK_WIDGET(ctxt->cmd_vbox_msg), true, true, 0);

    // open directory
    hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    label = gtk_label_new("Open In Browser:");
    gtk_widget_set_halign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(label), GtkAlign::GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), false, true, 0);
    ctxt->open_browser = gtk_combo_box_text_new();
    gtk_widget_set_focus_on_click(GTK_WIDGET(ctxt->open_browser), false);

    std::string str;
    std::filesystem::path path;
    if (rset->plugin)
    {
        path = rset->plugin->path / rset->plugin->name;
    }
    else
    {
        path = vfs::user_dirs->program_config_dir() / "scripts" / rset->name;
    }
    str = std::format("Command Dir  $fm_cmd_dir  {}", dir_has_files(path) ? "" : "(no files)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->open_browser), str.data());

    if (rset->plugin)
    {
        path = vfs::user_dirs->program_config_dir() / "plugin-data" / mset->name;
    }
    else
    {
        path = vfs::user_dirs->program_config_dir() / "plugin-data" / rset->name;
    }

    str = std::format("Data Dir  $fm_cmd_data  {}", dir_has_files(path) ? "" : "(no files)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->open_browser), str.data());

    if (rset->plugin)
    {
        str = "Plugin Dir  $fm_plugin_dir";
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->open_browser), str.data());
    }
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(ctxt->open_browser), false, true, 8);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), false, true, 0);

    // show all
    gtk_widget_show_all(GTK_WIDGET(ctxt->dlg));

    // load values  ========================================================
    // type
    item_prop::context::item_type item_type = item_prop::context::item_type::invalid;

    std::string item_type_str;
    if (set->tool > xset::tool::custom)
    {
        item_type_str =
            std::format("Built-In Toolbar Item: {}", xset_get_builtin_toolitem_label(set->tool));
    }
    else if (rset->menu_style == xset::menu::submenu)
    {
        item_type_str = "Submenu";
    }
    else if (rset->menu_style == xset::menu::sep)
    {
        item_type_str = "Separator";
    }
    else if (set->lock)
    {
        // built-in
        item_type_str = "Built-In Command";
    }
    else
    {
        // custom command
        for (const std::string_view item_type2 : item_types)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->item_type), item_type2.data());
        }
        xset::cmd x = rset->x ? xset::cmd(std::stoi(rset->x.value())) : xset::cmd::line;

        switch (x)
        {
            case xset::cmd::line:
            case xset::cmd::script:
                item_type = item_prop::context::item_type::command;
                break;
            case xset::cmd::app:
                item_type = item_prop::context::item_type::app;
                break;
            case xset::cmd::bookmark:
                item_type = item_prop::context::item_type::bookmark;
                break;
            case xset::cmd::invalid:
                item_type = item_prop::context::item_type::invalid;
                break;
        }

        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->item_type),
                                 item_type != item_prop::context::item_type::invalid
                                     ? magic_enum::enum_integer(item_type)
                                     : -1);
        // g_signal_connect( G_OBJECT( ctxt->item_type ), "changed",
        //                  G_CALLBACK( on_item_type_changed ), ctxt );
    }
    if (!item_type_str.empty())
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ctxt->item_type), item_type_str.data());
        gtk_combo_box_set_active(GTK_COMBO_BOX(ctxt->item_type), 0);
        gtk_widget_set_sensitive(ctxt->item_type, false);
    }

    if (!set->lock)
    {
        ctxt->temp_cmd_line = rset->line.value();
    }
    if (set->lock || rset->menu_style == xset::menu::submenu ||
        rset->menu_style == xset::menu::sep || set->tool > xset::tool::custom)
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
            gtk_text_buffer_set_text(buf, rset->z.value().data(), -1);
        }
    }
    ctxt->reset_command = true;

    // name
    if (rset->menu_style != xset::menu::sep)
    {
        if (set->menu_label)
        {
            gtk_entry_set_text(GTK_ENTRY(ctxt->item_name), set->menu_label.value().c_str());
        }
        else if (set->tool > xset::tool::custom)
        {
            gtk_entry_set_text(GTK_ENTRY(ctxt->item_name),
                               xset_get_builtin_toolitem_label(set->tool).c_str());
        }
    }
    else
    {
        gtk_widget_set_sensitive(ctxt->item_name, false);
    }
    // key
    if (rset->menu_style < xset::menu::submenu || set->tool == xset::tool::back_menu ||
        set->tool == xset::tool::fwd_menu)
    {
        std::string str2;
        xset_t keyset;
        if (set->shared_key)
        {
            keyset = xset_get(set->shared_key.value());
        }
        else
        {
            keyset = set;
        }
        str2 = xset_get_keyname(keyset, 0, 0);
        gtk_button_set_label(GTK_BUTTON(ctxt->item_key), str2.data());
    }
    else
    {
        gtk_widget_set_sensitive(ctxt->item_key, false);
    }
    // icon
    if (rset->icon || mset->icon)
    {
        gtk_entry_set_text(GTK_ENTRY(ctxt->item_icon),
                           mset->icon ? mset->icon.value().c_str() : rset->icon.value().c_str());
    }
    gtk_widget_set_sensitive(ctxt->item_icon,
                             rset->menu_style != xset::menu::radio &&
                                 rset->menu_style != xset::menu::sep);
    // toolbar checkbox items have icon
    //( rset->menu_style != xset::menu::CHECK || rset->tool ) );
    gtk_widget_set_sensitive(ctxt->icon_choose_btn,
                             rset->menu_style != xset::menu::radio &&
                                 rset->menu_style != xset::menu::sep);

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
    if (set->tool != xset::tool::NOT)
    {
        // Hide Context tab
        gtk_widget_hide(gtk_notebook_get_nth_page(GTK_NOTEBOOK(ctxt->notebook), 1));
        // gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->show_tool ),
        //                              set->tool == xset::b::XSET_B_TRUE );
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
    {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(ctxt->notebook), page);
    }
    else
    {
        gtk_widget_grab_focus(ctxt->set->plugin ? ctxt->item_icon : ctxt->item_name);
    }

    i32 response;
    while ((response = gtk_dialog_run(GTK_DIALOG(ctxt->dlg))))
    {
        bool exit_loop = false;
        switch (response)
        {
            case GtkResponseType::GTK_RESPONSE_OK:
                mset->context = context_build(ctxt);
                replace_item_props(ctxt);
                exit_loop = true;
                break;
            default:
                exit_loop = true;
                break;
        }
        if (exit_loop)
        {
            break;
        }
    }

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(ctxt->dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        xset_set(xset::name::context_dlg, xset::var::x, std::to_string(width));
        xset_set(xset::name::context_dlg, xset::var::y, std::to_string(height));
    }

    delete ctxt;
}
