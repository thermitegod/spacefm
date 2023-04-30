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

#include <glib.h>

#include <ztd/ztd.hxx>

#include "types.hxx"

#include "xset/xset-lookup.hxx"

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
    COLORDLG, // deprecated
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
    XSet(std::string_view name, XSetName xset_name);
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
    u32 key{0};                            // saved
    u32 keymod{0};                         // saved
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

#if defined(XSET_GETTER_SETTER)
  public:
    // get/set

    const char* get_name() const noexcept;

    bool is_xset_name(XSetName val) noexcept;
    bool is_xset_name(const std::vector<XSetName>& val) noexcept;
    XSetName get_xset_name() const noexcept;

    bool is_b(XSetB bval) noexcept;
    bool get_b() const noexcept;
    void set_b(bool bval) noexcept;
    void set_b(XSetB bval) noexcept;

    char* get_s() const noexcept;
    i32 get_s_int() const noexcept;
    void set_s(const char* val) noexcept;
    void set_s(const std::string& val) noexcept;

    char* get_x() const noexcept;
    i32 get_x_int() const noexcept;
    void set_x(const char* val) noexcept;
    void set_x(const std::string& val) noexcept;

    char* get_y() const noexcept;
    i32 get_y_int() const noexcept;
    void set_y(const char* val) noexcept;
    void set_y(const std::string& val) noexcept;

    char* get_z() const noexcept;
    i32 get_z_int() const noexcept;
    void set_z(const char* val) noexcept;
    void set_z(const std::string& val) noexcept;

    bool get_disable() const noexcept;
    void set_disable(bool bval) noexcept;

    char* get_menu_label() const noexcept;
    void set_menu_label(const char* val) noexcept;
    void set_menu_label(const std::string& val) noexcept;
    void set_menu_label_custom(const char* val) noexcept;
    void set_menu_label_custom(const std::string& val) noexcept;

    bool is_menu_style(XSetMenu val) noexcept;
    bool is_menu_style(const std::vector<XSetMenu>& val) noexcept;
    XSetMenu get_menu_style() const noexcept;
    void set_menu_style(XSetMenu val) noexcept;

    void set_cb(GFunc func, void* data) noexcept;

    void set_ob1(const char* ob, void* data) noexcept;
    void set_ob1(const char* ob, const char* data) noexcept;
    void set_ob1_int(const char* ob, i32 data) noexcept;
    void set_ob2(const char* ob, void* data) noexcept;

    u32 get_key() const noexcept;
    void set_key(u32 val) noexcept;

    u32 get_keymod() const noexcept;
    void set_keymod(u32 val) noexcept;

    char* get_shared_key() const noexcept;
    void set_shared_key(const char* val) noexcept;
    void set_shared_key(const std::string& val) noexcept;

    char* get_icon() const noexcept;
    void set_icon(const char* val) noexcept;
    void set_icon(const std::string& val) noexcept;

    char* get_desc() const noexcept;
    void set_desc(const char* val) noexcept;
    void set_desc(const std::string& val) noexcept;

    char* get_title() const noexcept;
    void set_title(const char* val) noexcept;
    void set_title(const std::string& val) noexcept;

    char* get_next() const noexcept;
    void set_next(const char* val) noexcept;
    void set_next(const std::string& val) noexcept;

    char* get_context() const noexcept;
    void set_context(const char* val) noexcept;
    void set_context(const std::string& val) noexcept;

    bool is_tool(XSetTool val) noexcept;
    bool is_tool(const std::vector<XSetTool>& val) noexcept;
    XSetTool get_tool() const noexcept;
    void set_tool(XSetTool val) noexcept;

    bool get_lock() const noexcept;
    void set_lock(bool bval) noexcept;

    char* get_prev() const noexcept;
    void set_prev(const char* val) noexcept;
    void set_prev(const std::string& val) noexcept;

    char* get_parent() const noexcept;
    void set_parent(const char* val) noexcept;
    void set_parent(const std::string& val) noexcept;

    char* get_child() const noexcept;
    void set_child(const char* val) noexcept;
    void set_child(const std::string& val) noexcept;

    char* get_line() const noexcept;
    void set_line(const char* val) noexcept;
    void set_line(const std::string& val) noexcept;

    bool get_task() const noexcept;
    void set_task(bool bval) noexcept;

    bool get_task_pop() const noexcept;
    void set_task_pop(bool bval) noexcept;

    bool get_task_err() const noexcept;
    void set_task_err(bool bval) noexcept;

    bool get_task_out() const noexcept;
    void set_task_out(bool bval) noexcept;

    bool get_in_terminal() const noexcept;
    void set_in_terminal(bool bval) noexcept;

    bool get_keep_terminal() const noexcept;
    void set_keep_terminal(bool bval) noexcept;

    bool get_scroll_lock() const noexcept;
    void set_scroll_lock(bool bval) noexcept;

    char get_opener() const noexcept;
    void set_opener(char val) noexcept;

    bool get_plugin() const noexcept;
    void set_plugin(bool bval) noexcept;

    bool get_plugin_top() const noexcept;
    void set_plugin_top(bool bval) noexcept;

    char* get_plug_name() const noexcept;
    void set_plug_name(const char* val) noexcept;
    void set_plug_name(const std::string& val) noexcept;

    char* get_plug_dir() const noexcept;
    void set_plug_dir(const char* val) noexcept;
    void set_plug_dir(const std::string& val) noexcept;
#endif
};

// using xset_t = std::unique_ptr<XSet>;
using xset_t = ztd::raw_ptr<XSet>;

// all xsets
extern std::vector<xset_t> xsets;

void xset_remove(xset_t set);

// get/set //

xset_t xset_new(std::string_view name, XSetName xset_name) noexcept;

xset_t xset_get(XSetName name) noexcept;
xset_t xset_get(std::string_view name) noexcept;

xset_t xset_is(XSetName name) noexcept;
xset_t xset_is(std::string_view name) noexcept;

void xset_set(xset_t name, XSetVar var, std::string_view value) noexcept;
void xset_set(XSetName name, XSetVar var, std::string_view value) noexcept;
void xset_set(std::string_view name, XSetVar var, std::string_view value) noexcept;

void xset_set_var(xset_t set, XSetVar var, std::string_view value) noexcept;

// B
bool xset_get_b(xset_t set) noexcept;
bool xset_get_b(XSetName name) noexcept;
bool xset_get_b(std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, XSetPanel name) noexcept;
bool xset_get_b_panel_mode(panel_t panel, std::string_view name, MainWindowPanel mode) noexcept;
bool xset_get_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode) noexcept;

void xset_set_b(xset_t set, bool bval) noexcept;
void xset_set_b(XSetName name, bool bval) noexcept;
void xset_set_b(std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, XSetPanel name, bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, std::string_view name, MainWindowPanel mode,
                           bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode, bool bval) noexcept;

// S
char* xset_get_s(xset_t set) noexcept;
char* xset_get_s(XSetName name) noexcept;
char* xset_get_s(std::string_view name) noexcept;
char* xset_get_s_panel(panel_t panel, std::string_view name) noexcept;
char* xset_get_s_panel(panel_t panel, XSetPanel name) noexcept;

// X
char* xset_get_x(xset_t set) noexcept;
char* xset_get_x(XSetName name) noexcept;
char* xset_get_x(std::string_view name) noexcept;

// Y
char* xset_get_y(xset_t set) noexcept;
char* xset_get_y(XSetName name) noexcept;
char* xset_get_y(std::string_view name) noexcept;

// Z
char* xset_get_z(xset_t set) noexcept;
char* xset_get_z(XSetName name) noexcept;
char* xset_get_z(std::string_view name) noexcept;

// Panel
xset_t xset_get_panel(panel_t panel, std::string_view name) noexcept;
xset_t xset_get_panel(panel_t panel, XSetPanel name) noexcept;
xset_t xset_get_panel_mode(panel_t panel, std::string_view name, MainWindowPanel mode) noexcept;
xset_t xset_get_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode) noexcept;

void xset_set_panel(panel_t panel, std::string_view name, XSetVar var,
                    std::string_view value) noexcept;
void xset_set_panel(panel_t panel, XSetPanel name, XSetVar var, std::string_view value) noexcept;

// CB

void xset_set_cb(xset_t set, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(XSetName name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(std::string_view name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, std::string_view name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, XSetPanel name, GFunc cb_func, void* cb_data) noexcept;

void xset_set_ob1_int(xset_t set, const char* ob1, i32 ob1_int) noexcept;
void xset_set_ob1(xset_t set, const char* ob1, void* ob1_data) noexcept;
void xset_set_ob2(xset_t set, const char* ob2, void* ob2_data) noexcept;

// Int

i32 xset_get_int(xset_t set, XSetVar var) noexcept;
i32 xset_get_int(XSetName name, XSetVar var) noexcept;
i32 xset_get_int(std::string_view name, XSetVar var) noexcept;
i32 xset_get_int_panel(panel_t panel, std::string_view name, XSetVar var) noexcept;
i32 xset_get_int_panel(panel_t panel, XSetPanel name, XSetVar var) noexcept;
