/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
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

#pragma once

#include <string>

#include <array>
#include <vector>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <ztd/ztd.hxx>

#include "types.hxx"

#include "xset-lookup.hxx"

#define XSET(obj) (static_cast<xset_t>(obj))

// this determines time before item is selected by hover in single-click mode
#define SINGLE_CLICK_TIMEOUT 150

// This limits the small icon size for side panes and task list
#define PANE_MAX_ICON_SIZE 48

// delimiter used in config file for tabs
#define CONFIG_FILE_TABS_DELIM "///"

void load_settings();
void autosave_settings();
void save_settings(void* main_window_ptr);
void free_settings();
const char* xset_get_config_dir();
const char* xset_get_user_tmp_dir();

///////////////////////////////////////////////////////////////////////////////
// MOD extra settings below

enum class XSetVar
{
    S,
    B,
    X,
    Y,
    Z,
    KEY,
    KEYMOD,
    STYLE,
    DESC,
    TITLE,
    MENU_LABEL,
    ICN,
    MENU_LABEL_CUSTOM,
    ICON,
    SHARED_KEY,
    NEXT,
    PREV,
    PARENT,
    CHILD,
    CONTEXT,
    LINE,
    TOOL,
    TASK,
    TASK_POP,
    TASK_ERR,
    TASK_OUT,
    RUN_IN_TERMINAL,
    KEEP_TERMINAL,
    SCROLL_LOCK,
    DISABLE,
    OPENER
};

enum class XSetCMD
{
    LINE,
    SCRIPT,
    APP,
    BOOKMARK,
    INVALID // Must be last
};

enum class XSetMenu
{ // do not reorder - these values are saved in session files
    NORMAL,
    CHECK,
    STRING,
    RADIO,
    FILEDLG,
    FONTDLG,
    ICON,
    COLORDLG,
    CONFIRM,
    RESERVED_03,
    RESERVED_04,
    RESERVED_05,
    RESERVED_06,
    RESERVED_07,
    RESERVED_08,
    RESERVED_09,
    RESERVED_10,
    SUBMENU, // add new before submenu
    SEP
};

enum class XSetTool
{ // do not reorder - these values are saved in session files
  // also update builtin_tool_name builtin_tool_icon in settings.c
    NOT,
    CUSTOM,
    DEVICES,
    BOOKMARKS,
    TREE,
    HOME,
    DEFAULT,
    UP,
    BACK,
    BACK_MENU,
    FWD,
    FWD_MENU,
    REFRESH,
    NEW_TAB,
    NEW_TAB_HERE,
    SHOW_HIDDEN,
    SHOW_THUMB,
    LARGE_ICONS,
    INVALID // Must be last
};

enum class XSetJob
{
    KEY,
    ICON,
    LABEL,
    EDIT,
    EDIT_ROOT,
    LINE,
    SCRIPT,
    CUSTOM,
    TERM,
    KEEP,
    USER,
    TASK,
    POP,
    ERR,
    OUT,
    BOOKMARK,
    APP,
    COMMAND,
    SUBMENU,
    SUBMENU_BOOK,
    SEP,
    ADD_TOOL,
    IMPORT_FILE,
    IMPORT_GTK,
    CUT,
    COPY,
    PASTE,
    REMOVE,
    REMOVE_BOOK,
    NORMAL,
    CHECK,
    CONFIRM,
    DIALOG,
    MESSAGE,
    COPYNAME,
    PROP,
    PROP_CMD,
    IGNORE_CONTEXT,
    SCROLL,
    EXPORT,
    BROWSE_FILES,
    BROWSE_DATA,
    BROWSE_PLUGIN,
    HELP,
    HELP_NEW,
    HELP_ADD,
    HELP_BROWSE,
    HELP_STYLE,
    HELP_BOOK,
    TOOLTIPS,
    INVALID // Must be last
};

enum XSetB
{
    XSET_B_UNSET,
    XSET_B_TRUE,
    XSET_B_FALSE
};

enum class PluginJob
{
    INSTALL,
    COPY,
    REMOVE
};

enum class PluginUse
{
    HAND_ARC,
    HAND_FS,
    HAND_NET,
    HAND_FILE,
    BOOKMARKS,
    NORMAL
};

struct PtkFileBrowser;

struct XSet
{
    XSet(const std::string& name, XSetName xset_name);
    ~XSet();

    char* name{nullptr};
    XSetName xset_name;

    XSetB b{XSetB::XSET_B_UNSET};          // saved, tri-state enum 0=unset(false) 1=true 2=false
    char* s{nullptr};                      // saved
    char* x{nullptr};                      // saved
    char* y{nullptr};                      // saved
    char* z{nullptr};                      // saved, for menu_string locked, stores default
    bool disable{false};                   // not saved
    char* menu_label{nullptr};             // saved
    XSetMenu menu_style{XSetMenu::NORMAL}; // saved if ( !lock ), or read if locked
    GFunc cb_func{nullptr};                // not saved
    void* cb_data{nullptr};                // not saved
    char* ob1{nullptr};                    // not saved
    void* ob1_data{nullptr};               // not saved
    char* ob2{nullptr};                    // not saved
    void* ob2_data{nullptr};               // not saved
    PtkFileBrowser* browser{nullptr};      // not saved - set automatically
    unsigned int key{0};                   // saved
    unsigned int keymod{0};                // saved
    char* shared_key{nullptr};             // not saved
    char* icon{nullptr};                   // saved
    char* desc{nullptr};                   // saved if ( !lock ), or read if locked
    char* title{nullptr};                  // saved if ( !lock ), or read if locked
    char* next{nullptr};                   // saved
    char* context{nullptr};                // saved
    XSetTool tool{XSetTool::NOT};          // saved
    bool lock{true};                       // not saved

    // Custom Command ( !lock )
    char* prev{nullptr};   // saved
    char* parent{nullptr}; // saved
    char* child{nullptr};  // saved
    char* line{nullptr};   // saved or help if lock
    // x = XSetCMD::LINE..XSetCMD::BOOKMARK
    // y = user
    // z = custom executable
    bool task{false};          // saved
    bool task_pop{false};      // saved
    bool task_err{false};      // saved
    bool task_out{false};      // saved
    bool in_terminal{false};   // saved, or save menu_label if lock
    bool keep_terminal{false}; // saved, or save icon if lock
    bool scroll_lock{false};   // saved
    char opener{0};            // saved

    // Plugin (not saved at all)
    bool plugin{false};       // not saved
    bool plugin_top{false};   // not saved
    char* plug_name{nullptr}; // not saved
    char* plug_dir{nullptr};  // not saved
};

// using xset_t = std::unique_ptr<XSet>;
using xset_t = ztd::raw_ptr<XSet>;

// cache these for speed in event handlers
struct EventHandler
{
    xset_t win_focus{nullptr};
    xset_t win_move{nullptr};
    xset_t win_click{nullptr};
    xset_t win_key{nullptr};
    xset_t win_close{nullptr};
    xset_t pnl_show{nullptr};
    xset_t pnl_focus{nullptr};
    xset_t pnl_sel{nullptr};
    xset_t tab_new{nullptr};
    xset_t tab_chdir{nullptr};
    xset_t tab_focus{nullptr};
    xset_t tab_close{nullptr};
    xset_t device{nullptr};
};

extern EventHandler event_handler;

extern std::vector<xset_t> xsets;

// instance-wide command history
extern std::vector<std::string> xset_cmd_history;

struct XSetContext
{
    XSetContext();
    ~XSetContext();

    bool valid{false};
    std::array<char*, 40> var;
};

void xset_set_window_icon(GtkWindow* win);

xset_t xset_is(XSetName name);
xset_t xset_is(const std::string& name);

xset_t xset_get(XSetName name);
xset_t xset_get(const std::string& name);

xset_t xset_get_panel(panel_t panel, const std::string& name);
xset_t xset_get_panel(panel_t panel, XSetPanel name);
xset_t xset_get_panel_mode(panel_t panel, const std::string& name, char mode);
xset_t xset_get_panel_mode(panel_t panel, XSetPanel name, char mode);

int xset_get_int(XSetName name, XSetVar var);
int xset_get_int(const std::string& name, XSetVar var);
int xset_get_int_panel(panel_t panel, const std::string& name, XSetVar var);
int xset_get_int_panel(panel_t panel, XSetPanel name, XSetVar var);

char* xset_get_s(XSetName name);
char* xset_get_s(const std::string& name);
char* xset_get_s_panel(panel_t panel, const std::string& name);
char* xset_get_s_panel(panel_t panel, XSetPanel name);

bool xset_get_b(XSetName name);
bool xset_get_b(const std::string& name);
bool xset_get_b_panel(panel_t panel, const std::string& name);
bool xset_get_b_panel(panel_t panel, XSetPanel name);
bool xset_get_b_panel_mode(panel_t panel, const std::string& name, char mode);
bool xset_get_b_panel_mode(panel_t panel, XSetPanel name, char mode);

xset_t xset_set_b(XSetName name, bool bval);
xset_t xset_set_b(const std::string& name, bool bval);
xset_t xset_set_b_panel(panel_t panel, const std::string& name, bool bval);
xset_t xset_set_b_panel(panel_t panel, XSetPanel name, bool bval);
xset_t xset_set_b_panel_mode(panel_t panel, const std::string& name, char mode, bool bval);
xset_t xset_set_b_panel_mode(panel_t panel, XSetPanel name, char mode, bool bval);

xset_t xset_set_panel(panel_t panel, const std::string& name, XSetVar var,
                      const std::string& value);
xset_t xset_set_panel(panel_t panel, XSetPanel name, XSetVar var, const std::string& value);

xset_t xset_set_cb(XSetName name, GFunc cb_func, void* cb_data);
xset_t xset_set_cb(const std::string& name, GFunc cb_func, void* cb_data);
xset_t xset_set_cb_panel(panel_t panel, const std::string& name, GFunc cb_func, void* cb_data);
xset_t xset_set_cb_panel(panel_t panel, XSetPanel name, GFunc cb_func, void* cb_data);

xset_t xset_set(XSetName name, XSetVar var, const std::string& value);
xset_t xset_set(const std::string& name, XSetVar var, const std::string& value);
xset_t xset_set_var(xset_t set, XSetVar var, const std::string& value);

void xset_set_key(GtkWidget* parent, xset_t set);

xset_t xset_set_ob1_int(xset_t set, const char* ob1, int ob1_int);
xset_t xset_set_ob1(xset_t set, const char* ob1, void* ob1_data);
xset_t xset_set_ob2(xset_t set, const char* ob2, void* ob2_data);

XSetContext* xset_context_new();
xset_t xset_get_plugin_mirror(xset_t set);
char* xset_custom_get_script(xset_t set, bool create);
const std::string xset_get_keyname(xset_t set, int key_val, int key_mod);

xset_t xset_custom_new();
void xset_custom_delete(xset_t set, bool delete_next);
xset_t xset_custom_remove(xset_t set);
char* xset_custom_get_app_name_icon(xset_t set, GdkPixbuf** icon, int icon_size);
GdkPixbuf* xset_custom_get_bookmark_icon(xset_t set, int icon_size);
void xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set);

GtkWidget* xset_design_show_menu(GtkWidget* menu, xset_t set, xset_t book_insert,
                                 unsigned int button, std::time_t time);
void xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                   const char* elements);
GtkWidget* xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu,
                             GtkAccelGroup* accel_group, xset_t set);
GtkWidget* xset_get_image(const char* icon, GtkIconSize icon_size);

xset_t xset_find_custom(const std::string& search);

int xset_msg_dialog(GtkWidget* parent, GtkMessageType action, const std::string& title,
                    GtkButtonsType buttons, const std::string& msg1, const std::string& msg2);
int xset_msg_dialog(GtkWidget* parent, GtkMessageType action, const std::string& title,
                    GtkButtonsType buttons, const std::string& msg1);
bool xset_text_dialog(GtkWidget* parent, const std::string& title, const std::string& msg1,
                      const std::string& msg2, const char* defstring, char** answer,
                      const std::string& defreset, bool edit_care);

void xset_menu_cb(GtkWidget* item, xset_t set);
bool xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data);
char* xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const char* title,
                       const char* deffolder, const char* deffile);
void xset_edit(GtkWidget* parent, const char* path, bool force_root, bool no_root);
void xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                       xset_t set_parent, bool show_tooltips);
GtkTextView* multi_input_new(GtkScrolledWindow* scrolled, const char* text);
void multi_input_select_region(GtkWidget* input, int start, int end);
char* multi_input_get_text(GtkWidget* input);

std::vector<xset_t> xset_get_plugins();
void xset_clear_plugins(std::vector<xset_t>& plugins);

void install_plugin_file(void* main_win, GtkWidget* handler_dlg, const std::string& path,
                         const std::string& plug_dir, PluginJob job, xset_t insert_set);
xset_t xset_import_plugin(const char* plug_dir, int* use);
void clean_plugin_mirrors();
bool xset_opener(PtkFileBrowser* file_browser, char job);
const char* xset_get_builtin_toolitem_label(XSetTool tool_type);
char* xset_icon_chooser_dialog(GtkWindow* parent, const char* def_icon);
