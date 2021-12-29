/*
 * SpaceFM settings.hxx
 *
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <string>

#include <vector>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "ptk/ptk-file-browser.hxx"

#define XSET(obj) (static_cast<XSet*>(obj))

// this determines time before item is selected by hover in single-click mode
#define SINGLE_CLICK_TIMEOUT 150

// This limits the small icon size for side panes and task list
#define PANE_MAX_ICON_SIZE 48

struct AppSettings
{
    // General Settings
    bool show_thumbnail{false};
    int max_thumb_size{8 << 20};

    int big_icon_size{48};
    int small_icon_size{22};
    int tool_icon_size{0};

    bool single_click{false};
    bool no_single_hover{false};

    bool no_execute{true};
    bool no_confirm{false};
    // bool no_confirm_delete{false}; // probably too dangerous
    bool no_confirm_trash{true};
    bool load_saved_tabs{true};
    std::string date_format{""};

    // Sort by name, size, time
    int sort_order{0};
    // ascending, descending
    int sort_type{0};

    // Window State
    int width{640};
    int height{480};
    bool maximized{false};

    // Interface
    bool always_show_tabs{true};
    bool show_close_tab_buttons{false};

    // Units
    bool use_si_prefix{false};
};

extern AppSettings app_settings;

struct ConfigSettings
{
    const char* terminal_su{nullptr};
    const char* tmp_dir{nullptr};

    const char* font_view_icon{nullptr};
    const char* font_view_compact{nullptr};
    const char* font_general{nullptr}; // NOOP

    bool git_backed_settings{true};
};

extern ConfigSettings config_settings;

void load_conf();
void load_settings(const char* config_dir);
void save_settings(void* main_window_ptr);
void free_settings();
const char* xset_get_config_dir();
const char* xset_get_user_tmp_dir();

///////////////////////////////////////////////////////////////////////////////
// MOD extra settings below

enum XSetSetSet
{
    XSET_SET_SET_S,
    XSET_SET_SET_B,
    XSET_SET_SET_X,
    XSET_SET_SET_Y,
    XSET_SET_SET_Z,
    XSET_SET_SET_KEY,
    XSET_SET_SET_KEYMOD,
    XSET_SET_SET_STYLE,
    XSET_SET_SET_DESC,
    XSET_SET_SET_TITLE,
    XSET_SET_SET_LBL,
    XSET_SET_SET_ICN,
    XSET_SET_SET_LABEL,
    XSET_SET_SET_ICON,
    XSET_SET_SET_SHARED_KEY,
    XSET_SET_SET_NEXT,
    XSET_SET_SET_PREV,
    XSET_SET_SET_PARENT,
    XSET_SET_SET_CHILD,
    XSET_SET_SET_CXT,
    XSET_SET_SET_LINE,
    XSET_SET_SET_TOOL,
    XSET_SET_SET_TASK,
    XSET_SET_SET_TASK_POP,
    XSET_SET_SET_TASK_ERR,
    XSET_SET_SET_TASK_OUT,
    XSET_SET_SET_TERM,
    XSET_SET_SET_KEEP,
    XSET_SET_SET_SCROLL,
    XSET_SET_SET_DISABLE,
    XSET_SET_SET_OP
};

enum XSetB
{
    XSET_B_UNSET,
    XSET_B_TRUE,
    XSET_B_FALSE
};

enum XSetCMD
{
    XSET_CMD_LINE,
    XSET_CMD_SCRIPT,
    XSET_CMD_APP,
    XSET_CMD_BOOKMARK
};

enum XSetMenu
{ // do not renumber - these values are saved in session files
    XSET_MENU_NORMAL,
    XSET_MENU_CHECK,
    XSET_MENU_STRING,
    XSET_MENU_RADIO,
    XSET_MENU_FILEDLG,
    XSET_MENU_FONTDLG,
    XSET_MENU_ICON,
    XSET_MENU_COLORDLG,
    XSET_MENU_CONFIRM,
    XSET_MENU_DUMMY3,
    XSET_MENU_DUMMY4,
    XSET_MENU_DUMMY5,
    XSET_MENU_DUMMY6,
    XSET_MENU_DUMMY7,
    XSET_MENU_DUMMY8,
    XSET_MENU_DUMMY9,
    XSET_MENU_DUMMY10,
    XSET_MENU_SUBMENU, // add new before submenu
    XSET_MENU_SEP
};

enum XSetTool
{ // do not reorder - these values are saved in session files
  // also update builtin_tool_name builtin_tool_icon in settings.c
    XSET_TOOL_NOT,
    XSET_TOOL_CUSTOM,
    XSET_TOOL_DEVICES,
    XSET_TOOL_BOOKMARKS,
    XSET_TOOL_TREE,
    XSET_TOOL_HOME,
    XSET_TOOL_DEFAULT,
    XSET_TOOL_UP,
    XSET_TOOL_BACK,
    XSET_TOOL_BACK_MENU,
    XSET_TOOL_FWD,
    XSET_TOOL_FWD_MENU,
    XSET_TOOL_REFRESH,
    XSET_TOOL_NEW_TAB,
    XSET_TOOL_NEW_TAB_HERE,
    XSET_TOOL_SHOW_HIDDEN,
    XSET_TOOL_SHOW_THUMB,
    XSET_TOOL_LARGE_ICONS,
    XSET_TOOL_INVALID // keep this always last
};

enum XSetJob
{
    XSET_JOB_KEY,
    XSET_JOB_ICON,
    XSET_JOB_LABEL,
    XSET_JOB_EDIT,
    XSET_JOB_EDIT_ROOT,
    XSET_JOB_LINE,
    XSET_JOB_SCRIPT,
    XSET_JOB_CUSTOM,
    XSET_JOB_TERM,
    XSET_JOB_KEEP,
    XSET_JOB_USER,
    XSET_JOB_TASK,
    XSET_JOB_POP,
    XSET_JOB_ERR,
    XSET_JOB_OUT,
    XSET_JOB_BOOKMARK,
    XSET_JOB_APP,
    XSET_JOB_COMMAND,
    XSET_JOB_SUBMENU,
    XSET_JOB_SUBMENU_BOOK,
    XSET_JOB_SEP,
    XSET_JOB_ADD_TOOL,
    XSET_JOB_IMPORT_FILE,
    XSET_JOB_IMPORT_GTK,
    XSET_JOB_CUT,
    XSET_JOB_COPY,
    XSET_JOB_PASTE,
    XSET_JOB_REMOVE,
    XSET_JOB_REMOVE_BOOK,
    XSET_JOB_NORMAL,
    XSET_JOB_CHECK,
    XSET_JOB_CONFIRM,
    XSET_JOB_DIALOG,
    XSET_JOB_MESSAGE,
    XSET_JOB_COPYNAME,
    XSET_JOB_PROP,
    XSET_JOB_PROP_CMD,
    XSET_JOB_IGNORE_CONTEXT,
    XSET_JOB_SCROLL,
    XSET_JOB_EXPORT,
    XSET_JOB_BROWSE_FILES,
    XSET_JOB_BROWSE_DATA,
    XSET_JOB_BROWSE_PLUGIN,
    XSET_JOB_HELP,
    XSET_JOB_HELP_NEW,
    XSET_JOB_HELP_ADD,
    XSET_JOB_HELP_BROWSE,
    XSET_JOB_HELP_STYLE,
    XSET_JOB_HELP_BOOK,
    XSET_JOB_TOOLTIPS
};

enum PluginJob
{
    PLUGIN_JOB_INSTALL,
    PLUGIN_JOB_COPY,
    PLUGIN_JOB_REMOVE
};

enum PluginUse
{
    PLUGIN_USE_HAND_ARC,
    PLUGIN_USE_HAND_FS,
    PLUGIN_USE_HAND_NET,
    PLUGIN_USE_HAND_FILE,
    PLUGIN_USE_BOOKMARKS,
    PLUGIN_USE_NORMAL
};

struct XSet
{
    char* name;
    XSetB b; // tri-state 0=unset(false) 1=true 2=false
    char* s;
    char* x;
    char* y;
    char* z;      // for menu_string locked, stores default
    bool disable; // not saved, default false
    char* menu_label;
    XSetMenu menu_style; // not saved or read if locked
    char* icon;
    GFunc cb_func;           // not saved
    void* cb_data;           // not saved
    char* ob1;               // not saved
    void* ob1_data;          // not saved
    char* ob2;               // not saved
    void* ob2_data;          // not saved
    PtkFileBrowser* browser; // not saved - set automatically
    unsigned int key;
    unsigned int keymod;
    char* shared_key; // not saved
    char* desc;       // not saved or read if locked
    char* title;      // not saved or read if locked
    char* next;
    char* context;
    unsigned char tool; // 0=not 1=true 2=false
    bool lock;          // not saved, default true

    // Custom Command ( !lock )
    char* prev;
    char* parent;
    char* child;
    char* line; // or help if lock
    // x = XSET_CMD_LINE..XSET_CMD_BOOKMARK
    // y = user
    // z = custom executable
    XSetB task;
    XSetB task_pop;
    XSetB task_err;
    XSetB task_out;
    XSetB in_terminal;   // or save menu_label if lock
    XSetB keep_terminal; // or save icon if lock
    XSetB scroll_lock;
    char opener;

    // Plugin (not saved at all)
    bool plugin;
    bool plugin_top;
    char* plug_name;
    char* plug_dir;
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
extern GList* xset_cmd_history;

struct XSetContext
{
    bool valid;
    char* var[40];
};

void xset_set_window_icon(GtkWindow* win);
XSet* xset_get(const char* name);
char* xset_get_s(const char* name);
bool xset_get_b(const char* name);
XSet* xset_get_panel(int panel, const char* name);
char* xset_get_s_panel(int panel, const char* name);
bool xset_get_b_panel(int panel, const char* name);
XSet* xset_set_b(const char* name, bool bval);
XSet* xset_set_b_panel(int panel, const char* name, bool bval);
int xset_get_int(const char* name, const char* var);
int xset_get_int_panel(int panel, const char* name, const char* var);
XSet* xset_set_panel(int panel, const char* name, const char* var, const char* value);
XSet* xset_set_cb_panel(int panel, const char* name, GFunc cb_func, void* cb_data);
// bool xset_get_b_set(XSet* set);
XSet* xset_get_panel_mode(int panel, const char* name, char mode);
bool xset_get_b_panel_mode(int panel, const char* name, char mode);
XSet* xset_set_b_panel_mode(int panel, const char* name, char mode, bool bval);

XSetContext* xset_context_new();
XSet* xset_get_plugin_mirror(XSet* set);
char* xset_custom_get_script(XSet* set, bool create);
char* xset_get_keyname(XSet* set, int key_val, int key_mod);
void xset_set_key(GtkWidget* parent, XSet* set);

XSet* xset_set(const char* name, const char* var, const char* value);
XSet* xset_set_set(XSet* set, int var, const char* value);
void xset_custom_delete(XSet* set, bool delete_next);
// void xset_custom_activate(GtkWidget* item, XSet* set);
XSet* xset_custom_remove(XSet* set);
char* xset_custom_get_app_name_icon(XSet* set, GdkPixbuf** icon, int icon_size);
GdkPixbuf* xset_custom_get_bookmark_icon(XSet* set, int icon_size);
void xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, XSet* set);
GtkWidget* xset_design_show_menu(GtkWidget* menu, XSet* set, XSet* book_insert, unsigned int button,
                                 uint32_t time);
void xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                   char* elements);
GtkWidget* xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu,
                             GtkAccelGroup* accel_group, XSet* set);
GtkWidget* xset_get_image(const char* icon, GtkIconSize icon_size);
XSet* xset_set_cb(const char* name, GFunc cb_func, void* cb_data);
XSet* xset_set_ob1_int(XSet* set, const char* ob1, int ob1_int);
XSet* xset_set_ob1(XSet* set, const char* ob1, void* ob1_data);
XSet* xset_set_ob2(XSet* set, const char* ob2, void* ob2_data);
XSet* xset_is(const char* name);
XSet* xset_find_custom(const char* search);

void xset_menu_cb(GtkWidget* item, XSet* set);
bool xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data);
bool xset_text_dialog(GtkWidget* parent, const char* title, bool large, const char* msg1,
                      const char* msg2, const char* defstring, char** answer, const char* defreset,
                      bool edit_care);
char* xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const char* title,
                       const char* deffolder, const char* deffile);
void xset_edit(GtkWidget* parent, const char* path, bool force_root, bool no_root);
void xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                       XSet* set_parent, bool show_tooltips);
int xset_msg_dialog(GtkWidget* parent, int action, const char* title, int buttons, const char* msg1,
                    const char* msg2);
GtkTextView* multi_input_new(GtkScrolledWindow* scrolled, const char* text);
void multi_input_select_region(GtkWidget* input, int start, int end);
char* multi_input_get_text(GtkWidget* input);
XSet* xset_custom_new();
bool write_root_settings(std::string& buf, const char* path);
GList* xset_get_plugins(bool included);
void install_plugin_file(void* main_win, GtkWidget* handler_dlg, const char* path,
                         const char* plug_dir, int job, XSet* insert_set);
XSet* xset_import_plugin(const char* plug_dir, int* use);
void clean_plugin_mirrors();
bool xset_opener(PtkFileBrowser* file_browser, char job);
const char* xset_get_builtin_toolitem_label(unsigned char tool_type);
char* xset_icon_chooser_dialog(GtkWindow* parent, const char* def_icon);
