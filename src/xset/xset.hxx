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
    XSet(const std::string_view name, XSetName xset_name);
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
    bool plugin{false};               // not saved
    bool plugin_top{false};           // not saved
    char* plug_name{nullptr};         // not saved
    std::filesystem::path plug_dir{}; // not saved
};

// using xset_t = std::unique_ptr<XSet>;
using xset_t = ztd::raw_ptr<XSet>;

// all xsets
extern std::vector<xset_t> xsets;

void xset_remove(xset_t set);

// get/set //

xset_t xset_new(const std::string_view name, XSetName xset_name) noexcept;

xset_t xset_get(XSetName name) noexcept;
xset_t xset_get(const std::string_view name) noexcept;

xset_t xset_is(XSetName name) noexcept;
xset_t xset_is(const std::string_view name) noexcept;

void xset_set(xset_t name, XSetVar var, const std::string_view value) noexcept;
void xset_set(XSetName name, XSetVar var, const std::string_view value) noexcept;
void xset_set(const std::string_view name, XSetVar var, const std::string_view value) noexcept;

void xset_set_var(xset_t set, XSetVar var, const std::string_view value) noexcept;

// B
bool xset_get_b(xset_t set) noexcept;
bool xset_get_b(XSetName name) noexcept;
bool xset_get_b(const std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, const std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, XSetPanel name) noexcept;
bool xset_get_b_panel_mode(panel_t panel, const std::string_view name,
                           MainWindowPanel mode) noexcept;
bool xset_get_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode) noexcept;

void xset_set_b(xset_t set, bool bval) noexcept;
void xset_set_b(XSetName name, bool bval) noexcept;
void xset_set_b(const std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, const std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, XSetPanel name, bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, const std::string_view name, MainWindowPanel mode,
                           bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode, bool bval) noexcept;

// S
char* xset_get_s(xset_t set) noexcept;
char* xset_get_s(XSetName name) noexcept;
char* xset_get_s(const std::string_view name) noexcept;
char* xset_get_s_panel(panel_t panel, const std::string_view name) noexcept;
char* xset_get_s_panel(panel_t panel, XSetPanel name) noexcept;

// X
char* xset_get_x(xset_t set) noexcept;
char* xset_get_x(XSetName name) noexcept;
char* xset_get_x(const std::string_view name) noexcept;

// Y
char* xset_get_y(xset_t set) noexcept;
char* xset_get_y(XSetName name) noexcept;
char* xset_get_y(const std::string_view name) noexcept;

// Z
char* xset_get_z(xset_t set) noexcept;
char* xset_get_z(XSetName name) noexcept;
char* xset_get_z(const std::string_view name) noexcept;

// Panel
xset_t xset_get_panel(panel_t panel, const std::string_view name) noexcept;
xset_t xset_get_panel(panel_t panel, XSetPanel name) noexcept;
xset_t xset_get_panel_mode(panel_t panel, const std::string_view name,
                           MainWindowPanel mode) noexcept;
xset_t xset_get_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode) noexcept;

void xset_set_panel(panel_t panel, const std::string_view name, XSetVar var,
                    const std::string_view value) noexcept;
void xset_set_panel(panel_t panel, XSetPanel name, XSetVar var,
                    const std::string_view value) noexcept;

// CB

void xset_set_cb(xset_t set, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(XSetName name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(const std::string_view name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, const std::string_view name, GFunc cb_func,
                       void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, XSetPanel name, GFunc cb_func, void* cb_data) noexcept;

void xset_set_ob1_int(xset_t set, const char* ob1, i32 ob1_int) noexcept;
void xset_set_ob1(xset_t set, const char* ob1, void* ob1_data) noexcept;
void xset_set_ob2(xset_t set, const char* ob2, void* ob2_data) noexcept;

// Int

i32 xset_get_int(xset_t set, XSetVar var) noexcept;
i32 xset_get_int(XSetName name, XSetVar var) noexcept;
i32 xset_get_int(const std::string_view name, XSetVar var) noexcept;
i32 xset_get_int_panel(panel_t panel, const std::string_view name, XSetVar var) noexcept;
i32 xset_get_int_panel(panel_t panel, XSetPanel name, XSetVar var) noexcept;
