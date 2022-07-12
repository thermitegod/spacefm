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

#include "types.hxx"

#include "xset.hxx"

#define XSET(obj) (static_cast<XSet*>(obj))

// this determines time before item is selected by hover in single-click mode
#define SINGLE_CLICK_TIMEOUT 150

// This limits the small icon size for side panes and task list
#define PANE_MAX_ICON_SIZE 48

// delimiter used in config file for tabs
#define CONFIG_FILE_TABS_DELIM "///"

void load_etc_conf();
void load_settings();
void autosave_settings();
void save_settings(void* main_window_ptr);
void free_settings();
const char* xset_get_config_dir();
const char* xset_get_user_tmp_dir();

///////////////////////////////////////////////////////////////////////////////
// MOD extra settings below

enum class XSetSetSet
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

    char* name;
    XSetName xset_name;

    XSetB b;                 // saved, tri-state enum 0=unset(false) 1=true 2=false
    char* s;                 // saved
    char* x;                 // saved
    char* y;                 // saved
    char* z;                 // saved, for menu_string locked, stores default
    bool disable;            // not saved, default false
    char* menu_label;        // saved
    XSetMenu menu_style;     // saved if ( !lock ), or read if locked
    char* icon;              // saved
    GFunc cb_func;           // not saved
    void* cb_data;           // not saved
    char* ob1;               // not saved
    void* ob1_data;          // not saved
    char* ob2;               // not saved
    void* ob2_data;          // not saved
    PtkFileBrowser* browser; // not saved - set automatically
    unsigned int key;        // saved
    unsigned int keymod;     // saved
    char* shared_key;        // not saved
    char* desc;              // saved if ( !lock ), or read if locked
    char* title;             // saved if ( !lock ), or read if locked
    char* next;              // saved
    char* context;           // saved
    XSetTool tool;           // saved
    bool lock;               // not saved, default true

    // Custom Command ( !lock )
    char* prev;   // saved
    char* parent; // saved
    char* child;  // saved
    char* line;   // saved or help if lock
    // x = XSetCMD::LINE..XSetCMD::BOOKMARK
    // y = user
    // z = custom executable
    bool task;          // saved
    bool task_pop;      // saved
    bool task_err;      // saved
    bool task_out;      // saved
    bool in_terminal;   // saved, or save menu_label if lock
    bool keep_terminal; // saved, or save icon if lock
    bool scroll_lock;   // saved
    char opener;        // saved

    // Plugin (not saved at all)
    bool plugin;     // not saved
    bool plugin_top; // not saved
    char* plug_name; // not saved
    char* plug_dir;  // not saved
};

// cache these for speed in event handlers
struct EventHandler
{
    XSet* win_focus{nullptr};
    XSet* win_move{nullptr};
    XSet* win_click{nullptr};
    XSet* win_key{nullptr};
    XSet* win_close{nullptr};
    XSet* pnl_show{nullptr};
    XSet* pnl_focus{nullptr};
    XSet* pnl_sel{nullptr};
    XSet* tab_new{nullptr};
    XSet* tab_chdir{nullptr};
    XSet* tab_focus{nullptr};
    XSet* tab_close{nullptr};
    XSet* device{nullptr};
};

extern EventHandler event_handler;

extern std::vector<XSet*> xsets;

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

XSet* xset_is(XSetName name);
XSet* xset_is(const std::string& name);

XSet* xset_get(XSetName name);
XSet* xset_get(const std::string& name);

XSet* xset_get_panel(panel_t panel, const std::string& name);
XSet* xset_get_panel_mode(panel_t panel, const std::string& name, char mode);

int xset_get_int(XSetName name, XSetSetSet var);
int xset_get_int(const std::string& name, XSetSetSet var);
int xset_get_int_panel(panel_t panel, const std::string& name, XSetSetSet var);

char* xset_get_s(XSetName name);
char* xset_get_s(const std::string& name);
char* xset_get_s_panel(panel_t panel, const std::string& name);

bool xset_get_b(XSetName name);
bool xset_get_b(const std::string& name);
bool xset_get_b_panel(panel_t panel, const std::string& name);
bool xset_get_b_panel_mode(panel_t panel, const std::string& name, char mode);

XSet* xset_set_b(XSetName name, bool bval);
XSet* xset_set_b(const std::string& name, bool bval);
XSet* xset_set_b_panel(panel_t panel, const std::string& name, bool bval);
XSet* xset_set_b_panel_mode(panel_t panel, const std::string& name, char mode, bool bval);

XSet* xset_set_panel(panel_t panel, const std::string& name, XSetSetSet var,
                     const std::string& value);

XSet* xset_set_cb(XSetName name, GFunc cb_func, void* cb_data);
XSet* xset_set_cb(const std::string& name, GFunc cb_func, void* cb_data);
XSet* xset_set_cb_panel(panel_t panel, const std::string& name, GFunc cb_func, void* cb_data);

XSet* xset_set(XSetName name, XSetSetSet var, const std::string& value);
XSet* xset_set(const std::string& name, XSetSetSet var, const std::string& value);
XSet* xset_set_set(XSet* set, XSetSetSet var, const std::string& value);

void xset_set_key(GtkWidget* parent, XSet* set);

XSet* xset_set_ob1_int(XSet* set, const char* ob1, int ob1_int);
XSet* xset_set_ob1(XSet* set, const char* ob1, void* ob1_data);
XSet* xset_set_ob2(XSet* set, const char* ob2, void* ob2_data);

XSetContext* xset_context_new();
XSet* xset_get_plugin_mirror(XSet* set);
char* xset_custom_get_script(XSet* set, bool create);
const std::string xset_get_keyname(XSet* set, int key_val, int key_mod);

XSet* xset_custom_new();
void xset_custom_delete(XSet* set, bool delete_next);
XSet* xset_custom_remove(XSet* set);
char* xset_custom_get_app_name_icon(XSet* set, GdkPixbuf** icon, int icon_size);
GdkPixbuf* xset_custom_get_bookmark_icon(XSet* set, int icon_size);
void xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, XSet* set);

GtkWidget* xset_design_show_menu(GtkWidget* menu, XSet* set, XSet* book_insert, unsigned int button,
                                 std::uint32_t time);
void xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                   const char* elements);
GtkWidget* xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu,
                             GtkAccelGroup* accel_group, XSet* set);
GtkWidget* xset_get_image(const char* icon, GtkIconSize icon_size);

XSet* xset_find_custom(const std::string& search);

int xset_msg_dialog(GtkWidget* parent, GtkMessageType action, const std::string& title,
                    GtkButtonsType buttons, const std::string& msg1, const std::string& msg2);
int xset_msg_dialog(GtkWidget* parent, GtkMessageType action, const std::string& title,
                    GtkButtonsType buttons, const std::string& msg1);
bool xset_text_dialog(GtkWidget* parent, const std::string& title, const std::string& msg1,
                      const std::string& msg2, const char* defstring, char** answer,
                      const std::string& defreset, bool edit_care);

void xset_menu_cb(GtkWidget* item, XSet* set);
bool xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data);
char* xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const char* title,
                       const char* deffolder, const char* deffile);
void xset_edit(GtkWidget* parent, const char* path, bool force_root, bool no_root);
void xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                       XSet* set_parent, bool show_tooltips);
GtkTextView* multi_input_new(GtkScrolledWindow* scrolled, const char* text);
void multi_input_select_region(GtkWidget* input, int start, int end);
char* multi_input_get_text(GtkWidget* input);

std::vector<XSet*> xset_get_plugins();
void xset_clear_plugins(std::vector<XSet*>& plugins);

void install_plugin_file(void* main_win, GtkWidget* handler_dlg, const std::string& path,
                         const std::string& plug_dir, PluginJob job, XSet* insert_set);
XSet* xset_import_plugin(const char* plug_dir, int* use);
void clean_plugin_mirrors();
bool xset_opener(PtkFileBrowser* file_browser, char job);
const char* xset_get_builtin_toolitem_label(XSetTool tool_type);
char* xset_icon_chooser_dialog(GtkWindow* parent, const char* def_icon);
