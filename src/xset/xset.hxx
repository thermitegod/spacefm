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

#include <optional>

#include <memory>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "types.hxx"

#include "xset/xset-lookup.hxx"

// need to forward declare to avoid circular header dependencies
struct PtkFileBrowser;

namespace xset
{
    struct XSet;
}
using xset_t = std::shared_ptr<xset::XSet>;

namespace xset
{
    enum class cmd
    {
        line,
        script,
        app,
        bookmark,
        invalid // Must be last
    };

    enum class menu
    {
        // do not reorder - these values are saved in session files
        normal,
        check,
        string,
        radio,
        reserved_00,
        reserved_01,
        reserved_02,
        reserved_03,
        reserved_04,
        reserved_05,
        reserved_06,
        reserved_07,
        reserved_08,
        reserved_09,
        reserved_10,
        reserved_11,
        reserved_12,
        submenu, // add new before submenu
        sep,
    };

    enum class tool
    {
        // do not reorder - these values are saved in session files
        // also update builtin_tool_name builtin_tool_icon in settings.c
        NOT, // TODO RENAME
        custom,
        devices,
        bookmarks,
        tree,
        home,
        DEFAULT, // TODO RENAME
        up,
        back,
        back_menu,
        fwd,
        fwd_menu,
        refresh,
        new_tab,
        new_tab_here,
        show_hidden,
        show_thumb,
        large_icons,
        invalid // Must be last
    };

    enum class job
    {
        key,
        add_tool,
        cut,
        copy,
        paste,
        remove,
        remove_book,
        invalid // Must be last
    };

    enum b
    {
        unset,
        xtrue,
        xfalse
    };

    struct XSet : public std::enable_shared_from_this<XSet>
    {
        XSet() = delete;
        XSet(const std::string_view set_name, xset::name xset_name);
        ~XSet();

        std::string name{};
        xset::name xset_name;

        xset::b b{xset::b::unset}; // saved, tri-state enum 0=unset(false) 1=true 2=false
        std::optional<std::string> s{std::nullopt}; // saved
        std::optional<std::string> x{std::nullopt}; // saved
        std::optional<std::string> y{std::nullopt}; // saved
        std::optional<std::string> z{std::nullopt}; // saved, for menu_string locked, stores default
        bool disable{false};                        // not saved
        std::optional<std::string> menu_label{std::nullopt}; // saved
        xset::menu menu_style{xset::menu::normal}; // saved if ( !lock ), or read if locked
        GFunc cb_func{nullptr};                    // not saved
        void* cb_data{nullptr};                    // not saved
        char* ob1{nullptr};                        // not saved
        void* ob1_data{nullptr};                   // not saved
        char* ob2{nullptr};                        // not saved
        void* ob2_data{nullptr};                   // not saved
        PtkFileBrowser* browser{nullptr};          // not saved - set automatically
        u32 key{0};                                // saved
        u32 keymod{0};                             // saved
        xset_t shared_key{nullptr};                // not saved

        std::optional<std::string> icon{std::nullopt};    // saved
        std::optional<std::string> desc{std::nullopt};    // saved if ( !lock ), or read if locked
        std::optional<std::string> title{std::nullopt};   // saved if ( !lock ), or read if locked
        std::optional<std::string> next{std::nullopt};    // saved
        std::optional<std::string> context{std::nullopt}; // saved
        xset::tool tool{xset::tool::NOT};                 // saved
        bool lock{true};                                  // not saved

        std::vector<xset::name> context_menu_entries{}; // not saved, in order

        // Custom Command ( !lock )
        std::optional<std::string> prev{std::nullopt};   // saved
        std::optional<std::string> parent{std::nullopt}; // saved
        std::optional<std::string> child{std::nullopt};  // saved
        std::optional<std::string> line{std::nullopt};   // saved or help if lock
        // x = xset::cmd::LINE..xset::cmd::BOOKMARK
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
    };
} // namespace xset

// all xsets
extern std::vector<xset_t> xsets;

// get/set //

const xset_t xset_new(const std::string_view name, xset::name xset_name) noexcept;

const xset_t xset_get(xset::name name) noexcept;
const xset_t xset_get(const std::string_view name) noexcept;

const xset_t xset_is(xset::name name) noexcept;
const xset_t xset_is(const std::string_view name) noexcept;

void xset_set(const xset_t& name, xset::var var, const std::string_view value) noexcept;
void xset_set(xset::name name, xset::var var, const std::string_view value) noexcept;
void xset_set(const std::string_view name, xset::var var, const std::string_view value) noexcept;

void xset_set_var(const xset_t& set, xset::var var, const std::string_view value) noexcept;

void xset_set_submenu(const xset_t& set, const std::vector<xset::name>& submenu_entries) noexcept;

// B
bool xset_get_b(const xset_t& set) noexcept;
bool xset_get_b(xset::name name) noexcept;
bool xset_get_b(const std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, const std::string_view name) noexcept;
bool xset_get_b_panel(panel_t panel, xset::panel name) noexcept;
bool xset_get_b_panel_mode(panel_t panel, const std::string_view name,
                           xset::main_window_panel mode) noexcept;
bool xset_get_b_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode) noexcept;

void xset_set_b(const xset_t& set, bool bval) noexcept;
void xset_set_b(xset::name name, bool bval) noexcept;
void xset_set_b(const std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, const std::string_view name, bool bval) noexcept;
void xset_set_b_panel(panel_t panel, xset::panel name, bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, const std::string_view name, xset::main_window_panel mode,
                           bool bval) noexcept;
void xset_set_b_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode,
                           bool bval) noexcept;

/**
 * Oneshot lookup functions
 */

// S
const std::optional<std::string> xset_get_s(const xset_t& set) noexcept;
const std::optional<std::string> xset_get_s(xset::name name) noexcept;
const std::optional<std::string> xset_get_s(const std::string_view name) noexcept;
const std::optional<std::string> xset_get_s_panel(panel_t panel,
                                                  const std::string_view name) noexcept;
const std::optional<std::string> xset_get_s_panel(panel_t panel, xset::panel name) noexcept;

// X
const std::optional<std::string> xset_get_x(const xset_t& set) noexcept;
const std::optional<std::string> xset_get_x(xset::name name) noexcept;
const std::optional<std::string> xset_get_x(const std::string_view name) noexcept;

// Y
const std::optional<std::string> xset_get_y(const xset_t& set) noexcept;
const std::optional<std::string> xset_get_y(xset::name name) noexcept;
const std::optional<std::string> xset_get_y(const std::string_view name) noexcept;

// Z
const std::optional<std::string> xset_get_z(const xset_t& set) noexcept;
const std::optional<std::string> xset_get_z(xset::name name) noexcept;
const std::optional<std::string> xset_get_z(const std::string_view name) noexcept;

// Panel
const xset_t xset_get_panel(panel_t panel, const std::string_view name) noexcept;
const xset_t xset_get_panel(panel_t panel, xset::panel name) noexcept;
const xset_t xset_get_panel_mode(panel_t panel, const std::string_view name,
                                 xset::main_window_panel mode) noexcept;
const xset_t xset_get_panel_mode(panel_t panel, xset::panel name,
                                 xset::main_window_panel mode) noexcept;

void xset_set_panel(panel_t panel, const std::string_view name, xset::var var,
                    const std::string_view value) noexcept;
void xset_set_panel(panel_t panel, xset::panel name, xset::var var,
                    const std::string_view value) noexcept;

// CB

void xset_set_cb(const xset_t& set, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(xset::name name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb(const std::string_view name, GFunc cb_func, void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, const std::string_view name, GFunc cb_func,
                       void* cb_data) noexcept;
void xset_set_cb_panel(panel_t panel, xset::panel name, GFunc cb_func, void* cb_data) noexcept;

void xset_set_ob1_int(const xset_t& set, const char* ob1, i32 ob1_int) noexcept;
void xset_set_ob1(const xset_t& set, const char* ob1, void* ob1_data) noexcept;
void xset_set_ob2(const xset_t& set, const char* ob2, void* ob2_data) noexcept;

// Int

i32 xset_get_int(const xset_t& set, xset::var var) noexcept;
i32 xset_get_int(xset::name name, xset::var var) noexcept;
i32 xset_get_int(const std::string_view name, xset::var var) noexcept;
i32 xset_get_int_panel(panel_t panel, const std::string_view name, xset::var var) noexcept;
i32 xset_get_int_panel(panel_t panel, xset::panel name, xset::var var) noexcept;
