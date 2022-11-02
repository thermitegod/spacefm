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

#pragma once

#include <string>
#include <string_view>

#include <vector>

#include <ztd/ztd.hxx>

#include "types.hxx"

#include "xset-lookup.hxx"

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
    SEP,
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

// need to forward declare to avoid circular header dependencies
struct PtkFileBrowser;

struct XSet
{
  public:
    XSet() = delete;
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

  public:
    // get/set

    const char* get_name();

    bool is_xset_name(XSetName val);
    bool is_xset_name(const std::vector<XSetName>& val);
    XSetName get_xset_name();

    bool is_b(XSetB bval);
    bool get_b();
    void set_b(bool bval);
    void set_b(XSetB bval);

    char* get_s();
    int get_s_int();
    void set_s(const char* val);
    void set_s(const std::string& val);

    char* get_x();
    int get_x_int();
    void set_x(const char* val);
    void set_x(const std::string& val);

    char* get_y();
    int get_y_int();
    void set_y(const char* val);
    void set_y(const std::string& val);

    char* get_z();
    int get_z_int();
    void set_z(const char* val);
    void set_z(const std::string& val);

    bool get_disable();
    void set_disable(bool bval);

    char* get_menu_label();
    void set_menu_label(const char* val);
    void set_menu_label(const std::string& val);
    void set_menu_label_custom(const char* val);
    void set_menu_label_custom(const std::string& val);

    bool is_menu_style(XSetMenu val);
    bool is_menu_style(const std::vector<XSetMenu>& val);
    XSetMenu get_menu_style();
    void set_menu_style(XSetMenu val);

    void set_cb(GFunc func, void* data);

    void set_ob1(const char* ob, void* data);
    void set_ob1(const char* ob, const char* data);
    void set_ob1_int(const char* ob, int data);
    void set_ob2(const char* ob, void* data);

    unsigned int get_key();
    void set_key(unsigned int val);

    unsigned int get_keymod();
    void set_keymod(unsigned int val);

    char* get_shared_key();
    void set_shared_key(const char* val);
    void set_shared_key(const std::string& val);

    char* get_icon();
    void set_icon(const char* val);
    void set_icon(const std::string& val);

    char* get_desc();
    void set_desc(const char* val);
    void set_desc(const std::string& val);

    char* get_title();
    void set_title(const char* val);
    void set_title(const std::string& val);

    char* get_next();
    void set_next(const char* val);
    void set_next(const std::string& val);

    char* get_context();
    void set_context(const char* val);
    void set_context(const std::string& val);

    bool is_tool(XSetTool val);
    bool is_tool(const std::vector<XSetTool>& val);
    XSetTool get_tool();
    void set_tool(XSetTool val);

    bool get_lock();
    void set_lock(bool bval);

    char* get_prev();
    void set_prev(const char* val);
    void set_prev(const std::string& val);

    char* get_parent();
    void set_parent(const char* val);
    void set_parent(const std::string& val);

    char* get_child();
    void set_child(const char* val);
    void set_child(const std::string& val);

    char* get_line();
    void set_line(const char* val);
    void set_line(const std::string& val);

    bool get_task();
    void set_task(bool bval);

    bool get_task_pop();
    void set_task_pop(bool bval);

    bool get_task_err();
    void set_task_err(bool bval);

    bool get_task_out();
    void set_task_out(bool bval);

    bool get_in_terminal();
    void set_in_terminal(bool bval);

    bool get_keep_terminal();
    void set_keep_terminal(bool bval);

    bool get_scroll_lock();
    void set_scroll_lock(bool bval);

    char get_opener();
    void set_opener(char val);

    bool get_plugin();
    void set_plugin(bool bval);

    bool get_plugin_top();
    void set_plugin_top(bool bval);

    char* get_plug_name();
    void set_plug_name(const char* val);
    void set_plug_name(const std::string& val);

    char* get_plug_dir();
    void set_plug_dir(const char* val);
    void set_plug_dir(const std::string& val);
};

// using xset_t = std::unique_ptr<XSet>;
using xset_t = ztd::raw_ptr<XSet>;

// all xsets
extern std::vector<xset_t> xsets;

// get/set //

xset_t xset_new(const std::string& name, XSetName xset_name);

xset_t xset_get(XSetName name);
xset_t xset_get(const std::string& name);

xset_t xset_is(XSetName name);
xset_t xset_is(const std::string& name);

xset_t xset_set(xset_t name, XSetVar var, const std::string& value);
xset_t xset_set(XSetName name, XSetVar var, const std::string& value);
xset_t xset_set(const std::string& name, XSetVar var, const std::string& value);

xset_t xset_set_var(xset_t set, XSetVar var, const std::string& value);

// B
bool xset_get_b(xset_t set);
bool xset_get_b(XSetName name);
bool xset_get_b(const std::string& name);
bool xset_get_b_set(xset_t set);
bool xset_get_b_panel(panel_t panel, const std::string& name);
bool xset_get_b_panel(panel_t panel, XSetPanel name);
bool xset_get_b_panel_mode(panel_t panel, const std::string& name, MainWindowPanel mode);
bool xset_get_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode);

xset_t xset_set_b(xset_t set, bool bval);
xset_t xset_set_b(XSetName name, bool bval);
xset_t xset_set_b(const std::string& name, bool bval);
xset_t xset_set_b_panel(panel_t panel, const std::string& name, bool bval);
xset_t xset_set_b_panel(panel_t panel, XSetPanel name, bool bval);
xset_t xset_set_b_panel_mode(panel_t panel, const std::string& name, MainWindowPanel mode,
                             bool bval);
xset_t xset_set_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode, bool bval);

// S
char* xset_get_s(xset_t set);
char* xset_get_s(XSetName name);
char* xset_get_s(const std::string& name);
char* xset_get_s_panel(panel_t panel, const std::string& name);
char* xset_get_s_panel(panel_t panel, XSetPanel name);

// X
char* xset_get_x(xset_t set);
char* xset_get_x(XSetName name);
char* xset_get_x(const std::string& name);

// Y
char* xset_get_y(xset_t set);
char* xset_get_y(XSetName name);
char* xset_get_y(const std::string& name);

// Z
char* xset_get_z(xset_t set);
char* xset_get_z(XSetName name);
char* xset_get_z(const std::string& name);

// Panel
xset_t xset_get_panel(panel_t panel, const std::string& name);
xset_t xset_get_panel(panel_t panel, XSetPanel name);
xset_t xset_get_panel_mode(panel_t panel, const std::string& name, MainWindowPanel mode);
xset_t xset_get_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode);

xset_t xset_set_panel(panel_t panel, const std::string& name, XSetVar var,
                      const std::string& value);
xset_t xset_set_panel(panel_t panel, XSetPanel name, XSetVar var, const std::string& value);

// CB

xset_t xset_set_cb(XSetName name, GFunc cb_func, void* cb_data);
xset_t xset_set_cb(const std::string& name, GFunc cb_func, void* cb_data);
xset_t xset_set_cb_panel(panel_t panel, const std::string& name, GFunc cb_func, void* cb_data);
xset_t xset_set_cb_panel(panel_t panel, XSetPanel name, GFunc cb_func, void* cb_data);

xset_t xset_set_ob1_int(xset_t set, const char* ob1, int ob1_int);
xset_t xset_set_ob1(xset_t set, const char* ob1, void* ob1_data);
xset_t xset_set_ob2(xset_t set, const char* ob2, void* ob2_data);

// Int

int xset_get_int_set(xset_t set, XSetVar var);

int xset_get_int(XSetName name, XSetVar var);
int xset_get_int(const std::string& name, XSetVar var);
int xset_get_int_panel(panel_t panel, const std::string& name, XSetVar var);
int xset_get_int_panel(panel_t panel, XSetPanel name, XSetVar var);
